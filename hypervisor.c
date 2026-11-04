#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define GUEST_MEM_SIZE 0x1000

static void fail(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

int main(void)
{
    int kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm_fd < 0)
        fail("open /dev/kvm");

    int api = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (api != KVM_API_VERSION) {
        fprintf(stderr, "Unsupported KVM API version: %d\n", api);
        return EXIT_FAILURE;
    }

    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
    if (vm_fd < 0)
        fail("KVM_CREATE_VM");

    void *memory = mmap(NULL, GUEST_MEM_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
        fail("mmap guest RAM");

    const uint8_t code[] = {0xB0, 0x2A, 0xE6, 0x10, 0xF4};
    memcpy(memory, code, sizeof(code));

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = GUEST_MEM_SIZE,
        .userspace_addr = (uint64_t)(uintptr_t)memory,
    };
    if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0)
        fail("KVM_SET_USER_MEMORY_REGION");

    int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    if (vcpu_fd < 0)
        fail("KVM_CREATE_VCPU");

    int run_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (run_size < (int)sizeof(struct kvm_run)) {
        fprintf(stderr, "Invalid KVM run mapping size\n");
        return EXIT_FAILURE;
    }
    struct kvm_run *run = mmap(NULL, (size_t)run_size,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, vcpu_fd, 0);
    if (run == MAP_FAILED)
        fail("mmap vCPU run");

    struct kvm_sregs sregs;
    if (ioctl(vcpu_fd, KVM_GET_SREGS, &sregs) < 0)
        fail("KVM_GET_SREGS");
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    if (ioctl(vcpu_fd, KVM_SET_SREGS, &sregs) < 0)
        fail("KVM_SET_SREGS");

    struct kvm_regs regs = {
        .rip = 0,
        .rflags = 0x2,
    };
    if (ioctl(vcpu_fd, KVM_SET_REGS, &regs) < 0)
        fail("KVM_SET_REGS");

    printf("Host: starting VM...\n");
    for (;;) {
        if (ioctl(vcpu_fd, KVM_RUN, 0) < 0)
            fail("KVM_RUN");

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            if (run->io.direction == KVM_EXIT_IO_OUT &&
                run->io.port == 0x10 &&
                run->io.size == 1 && run->io.count == 1) {
                uint8_t *value = (uint8_t *)run +
                                 run->io.data_offset;
                printf("Host: guest sent %u to port 0x10\n",
                       (unsigned)*value);
            } else {
                fprintf(stderr, "Unexpected guest I/O\n");
                return EXIT_FAILURE;
            }
            break;
        case KVM_EXIT_HLT:
            printf("Host: guest executed HLT. Done.\n");
            munmap(run, (size_t)run_size);
            munmap(memory, GUEST_MEM_SIZE);
            close(vcpu_fd);
            close(vm_fd);
            close(kvm_fd);
            return EXIT_SUCCESS;
        default:
            fprintf(stderr, "Unexpected VM exit: %u\n",
                    run->exit_reason);
            return EXIT_FAILURE;
        }
    }
}
