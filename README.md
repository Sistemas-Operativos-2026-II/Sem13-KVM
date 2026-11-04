# Laboratorio 14: hipervisor mínimo con KVM

En este laboratorio implementará una máquina virtual mínima mediante KVM en Linux x86_64. El objetivo es comprender cómo el programa anfitrión configura y ejecuta un invitado, y cómo atiende sus operaciones de entrada/salida y su detención. El archivo [`hypervisor.c`](hypervisor.c) contiene el programa presentado en la guía del laboratorio en LaTeX.

El invitado ejecuta tres instrucciones:

```asm
mov al, 42
out 0x10, al
hlt
```

Primero carga el valor `42` en el registro `AL`, luego lo envía al puerto `0x10` y, por último, se detiene. El programa anfitrión maneja las salidas de KVM por entrada/salida y por la instrucción `HLT` desde el espacio de usuario.

## Requisitos

- Linux x86_64 con acceso a `/dev/kvm` y permisos para el usuario que ejecutará el programa.
- GCC y las cabeceras de la API de Linux, incluido `linux/kvm.h`.
- Virtualización anidada habilitada si realiza el laboratorio dentro de otra máquina virtual.

## Compilación y ejecución

Abra una terminal en la carpeta que contiene `hypervisor.c` y compruebe la disponibilidad y los permisos del dispositivo KVM:

```bash
ls -l /dev/kvm
```

A continuación, compile y ejecute el programa con los siguientes comandos:

```bash
gcc -Wall -Wextra -O2 hypervisor.c -o hypervisor
./hypervisor
```

Si la ejecución se completa correctamente, obtendrá la siguiente salida:

```text
Host: starting VM...
Host: guest sent 42 to port 0x10
Host: guest executed HLT. Done.
```

## Actividad: modificar el valor enviado

Para que el invitado envíe `23` en lugar de `42`, localice el arreglo `code` en `hypervisor.c` y sustituya su segundo byte, `0x2A`, por `0x17`:

```c
const uint8_t code[] = {0xB0, 0x17, 0xE6, 0x10, 0xF4};
```

Compile y ejecute nuevamente el programa. Verifique que el mensaje indique que el invitado envió `23` al puerto `0x10`. Relacione este cambio con el valor que la instrucción `mov` carga en el registro `AL`.


