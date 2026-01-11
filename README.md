# 1Cotlin

1Cotlin programming language. Created and developed by subadminka & lphfs.

Минимальный язык для экспериментов на Windows x64. Компилируется в .exe собственным компилятором.

## Возможности
- print
- переменные через let и присваивание
- условия if/else
- repeat для повторения
- числа, строки, логика

## Пример

```1cotlin
let n = 5
print "привет"
repeat n {
    print n
}

if n > 2 {
    print "много"
} else {
    print "мало"
}
```

## Синтаксис

### Переменные

```1cotlin
let x = 10
x = x + 5
```

### Числа и логика

```1cotlin
let a = 3
let b = 7
print a * b
print a < b
print not false
```

### Условия

```1cotlin
let n = 2
if n == 2 {
    print "ok"
} else {
    print "no"
}
```

### Повторение

```1cotlin
repeat 3 {
    print "again"
}
```

### Строки

```1cotlin
print "hello"
```

Строки используются только в print. Переменным и арифметике доступны только числа и логика.

## Требования
- Windows x64
- Visual Studio 2022 (MSVC) или MinGW (gcc)

## Сборка компилятора

```powershell
cl /Fe:1cotlinc.exe main.c lexer.c parser.c sema.c codegen.c pe.c util.c
```

Если нет MSVC:

```powershell
gcc -O2 -o 1cotlinc.exe main.c lexer.c parser.c sema.c codegen.c pe.c util.c
```

## Компиляция .1c в .exe

```powershell
.\1cotlinc.exe examples\hello.1c
```

Выходной файл по умолчанию рядом с исходником: `examples\hello.exe`.
Можно задать имя вручную:

```powershell
.\1cotlinc.exe examples\hello.1c myprog.exe
```

## Запуск

```powershell
.\examples\hello.exe
```

## Заметки
- Файлы .1c можно сохранять в UTF-8 или UTF-16LE, компилятор читает оба.
- `print` печатает значение и перевод строки.
