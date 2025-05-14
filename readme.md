# Сборка библиотеки xiwrapper

```
git clone https://gitlab.ximc.ru/ximc/libxiwrapper
```
В исходном файле `wrapper.cpp` заменить угловые скобки строки в начале файла: `#include <tinythread.h>` на двойные кавычки, так
`#include "tinythread.h"`.

## Windows

Библиотеку собирать сначала с помощью CMake GUI, потом с помощью Visual Studio 2013, как и большинство наших проектов:

* создать каталог `Bindy` в удобном месте;
* скопировать в этот каталог `bindy.h`, `bindy-static.h` (из проекта Bindy, ветка `dev-1.0-libximc`);
* скопировать туда же `bindy.lib`, `bindy.dll` требуемой разряднсти (х32 или х64) (можно взять из релиза `libximc` или собрать самостоятельно);
* cкачать и установить CMake (https://cmake.org) и MS Visual Studio (13 и выше);
* запустить cmake-gui;
* указать путь до каталога `libxiwrapper` (с CMakeLists.txt) и путь `<до каталога libxiwrapper>/build` в поле для выходной сборочной директории;
* добавить переменную (нажать `Add entry`): имя - `BINDY_PATH`, тип - `string`, значение - `<путь до каталога Bindy>/Bindy`;
* `Configure` > `Visual Studio 12 2013`, выбрать разрядность проекта (х32 или x64, разрядность Bindy должна быть той же);
* `Generate`;
* собрать сгенерированное решение в Visual Studio.

## Linux, Mac OS

Библиотеку собрать, как и все наши проекты на CMake\make:

* создать каталог `Bindy` в удобном месте;
* скопировать в этот каталог `bindy.h`, `bindy-static.h` (из проекта Bindy, ветка `dev-1.0-libximc`);
* скопировать в этот каталог `libbindy.so` (Linux) или `libbindy.dylib` (Mac OS) (можно взять из релиза `libximc` или собрать самостоятельно);
* `mkdir build`
* `cd build`
* `cmake .. -DBINDY_PATH=<путь до Bindy>/Bindy`
* `make`