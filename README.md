# encrucijada ![version](https://img.shields.io/badge/version-1.0-blue)

Trabajo de la asignatura SSOOII en el 2º Curso del Grado de Ingeniería Informática USAL

**NOTA FINAL: 10** 

## Finalidad del proyecto 🚀

La finalidad de este trabajo es la organización en procesos (o hilos) dentro de un sistema operativo mediante el uso de sémaforos, memoría compartida o buzones.

Se utiliza una interfaz gráfica de coches y peatones para ver más visualmente el problema

### Pre-requisitos 📋

_Para instalar este programa necesitas tener instalado al menos:_

Un compilador de C y C++ en ambos sistemas
 
 En UNIX:
 
 _Biblioteca 32bits C_

    * Con la orden dpkg --print-architecture, comprobad que realmente tenéis un Linux de 64 bits. Debe aparecer, amd64.
    * Meted ahora la orden dpkg --print-foreign-architectures. Si entre la salida no aparece i386, debéis teclear: sudo dpkg --add-architecture i386
    * Ahora necesitáis tener las bibliotecas de 32 bits también instaladas. Lo lográis con: sudo apt-get install g++-multilib
    * Finalmente, podéis hacer una prueba para ver si todo funciona. Compilad vuestra práctica incluyendo la biblioteca de Linux de 
        32 bits y la opción -m32 en la línea  de compilación del gcc: gcc -m32...
    
    *Si la fase anterior no dio ningún error y os generó el ejecutable, probad a ejecutarlo. Si todo ha ido bien, debería ejecutarse sin problemas.

 
### Instalación 🔧

 En UNIX: 
 
 32bits  -- se incluye libcruce.a para solaris
  ```
  gcc cruce.c libcruce.a -o cruce
  ```
  
  64bits después instalar la biblioteca de 32 bits
  ```
  gcc -m32 cruce.c libcruce.a -o cruce
  ```

  En Windows:
  
  Creando un proyecto de VisualStudio
  
### Ejecución 🔧

  Para ejecutar hay que pasar el parametro de nº de Procesos y el de la velocidad de simulación
  
## Construido con 🛠️

_Estas son las herramientas usadas para el desarrollo_

* VisualStudioCode - Para la versión Unix
* VisualCode 2019 - Para la versión Windows

## Autores ✒️

Desarrollado por:

* **Ángel Picado Cuadrado** - [angelcubo01](https://github.com/angelcubo01)
* **David Salvador Aguado** -
* **Guillermo González Talaván** - Profesor 

## Ayuda

En caso de necesitar más información contactar con [angelcubo01](https://github.com/angelcubo01)
