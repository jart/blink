# Running SectorLISP in Blinkenlights

[SectorLISP](https://justine.lol/sectorlisp2/) is an tiniest garbage
collected programming environment in the world. It's written as an i8086
BIOS program that's compatible with the original IBM PC. You can run
SectorLISP in Blink, by passing the `-r` (REAL MODE) flag.

## Running SectorLISP

If you build Blink, then you can use the binaries provided for your
convenience in this folder.

```sh
make -j8 o//blink

# easy mode
o//blink/blinkenlights -r third_party/sectorlisp/sectorlisp-friendly.bin

# professional mode
o//blink/blinkenlights -r third_party/sectorlisp/sectorlisp.bin
```

Then press `c` to continue. The teletypewriter should become
highlighted, which means it's waiting for keystrokes.

## LISP Hello World

You can then type in code like the following, which interns an atom:

```lisp
(QUOTE HI)
```

The above code followed by the `ENTER` key will display:

```lisp
HI
```

For the purposes of readability in this tutorial, the above workflow
will be described as follows:

```lisp
>> (QUOTE HI)
HI
```

## Keyboard Control

Once you've pressed `c` (CONTINUE) the program TTY takes over control.
You can drop back into the debugger by pressing `CTRL-C`. For example,
if you want to exit Blink entirely while using SectorLISP, you'd press
`CTRL-C` (INTERRUPT) followed by `q` (QUIT).

One thing you may have noticed, is that SectorLISP's interning algorithm
is slow. You can speed up or slow down execution at any time, by
pressing `CTRL-C` (INTERRUPT) and, once in debugger mode, use `CTRL-T`
(TURBO) or `ALT-T` (SLOWMO) as many times as desired. Then, execution
may be resumed by `c` (CONTINUE) which hands control back over to the
SectorLISP program.

An alternative to pressing `c` (CONTINUE) is to press `s` (STEP). This
will allow you to go instruction by instruction. Be careful if you run
into an `INT` instruction, since `INT $0x16` for example, is your BIOS
keyboard service. In stepping mode, Blink will read one keystroke, and
then drop back into debugger control mode.

## LISP Data Structures

Here's an example of how you can build data structures:

```lisp
> (CONS (QUOTE A) (QUOTE B))
(A∙B)
```

## Recursive Functions

SectorLISP is purely expressional. It also doesn't have macros, which
are what make the modern LISP syntax most people are used to possible.
You instead need to write LISP the old fashioned way, as it is written
in John McCarthy's original paper.

Here's an example of a recursive function which finds the first atom in
the tree `((A) B C)`, which is `A`.

```lisp
>> ((LAMBDA (FF X) (FF X))
    (QUOTE (LAMBDA (X)
             (COND ((ATOM X) X)
                   ((QUOTE T) (FF (CAR X))))))
    (QUOTE ((A) B C)))
A
```

The friendly version of SectorLISP makes it possible to break giant
expressions like the above into smaller pieces. For example, the actual
function from the S-expression above could be defined as follows:

```lisp
(DEFINE FF .
  (LAMBDA (X)
    (COND ((ATOM X) X)
          ((QUOTE T) (FF (CAR X))))))
```

The friendly version also has the advantage of not exhibiting undefined
behavior upon mistyped keystrokes!

## On Real Mode

We call real mode "real" because it puts your virtual machine in a mode
that simulates non-virtualized memory ops. This adds a certain level of
fun when programming, because it makes things so much simpler than long
mode, which mandates that all memory operations be indirected through a
4-level radix trie.

The i8086 is still able to virtualize memory using segment registers:

- `%cs` (CODE SEGMENT)
- `%ds` (DATA SEGMENT)
- `%ss` (STACK SEGMENT)
- `%es` (EXTRA SEGMENT)

BIOS programs are loaded to the address 0x7c00. What SectorLISP does is
it sets all the segments to the load address, which effectively
redefines `NULL` to be equal to the base of the program image.
