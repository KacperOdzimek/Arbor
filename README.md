# Arbor

Arbor is a C99, data-driven, user-interface system, with little depedency (only standard c99 library).   

It allows retained layout generation, with immediate animations. Thanks to *instancing* mechanism, user can create reusable, interactive UI structures.  

Arbor is renderer agnostic as, instead of doing gpu api calls, it returns sorted lists of rendering primitives, your renderer can consume.

## Features

| Feature | Gain |
| --- | --- |
| Single header library | Easy compilation - no linking, no build systems |
| Render Agnostic | Arbor returns lists of rendering primitives, you can easily render with own renderer/engine |
| Data Driven | Arbor is all data - UI can be easily generated with code, or predefinied as a static array |
| Reusable Components | Using *instancing system*, user can define ui element once and use multiple times with diffrent images/texts/contents |
| Cursor Input Handling | Just pass mouse position and state, and buttons will work |
| Incremental layout invalidation | For big static structures, use *arb_invalidation_type* to block recreating subtree layout - usefull for long textes!
| Transform and clipping support | Everything in arbor can be arbitrary rotated, translated, scaled or trimmed
| Custom Node Types | User can define own types, if willing to |

## Documentation

| Document | Knowledge |
| --- | --- |
| [Design Reference](documentation/design_reference.md) | Learn how to use Arbor to define your application's UI. |
| [Contents Reference](documentation/contents_reference.md) | Learn Arbor node types and UI prefabs. |
| [Types Reference](documentation/contents_reference.md) | Learn how to define custom Arbot node type, for custom behavior |
| [Integration Reference](documentation/integration_reference.md)| Learn how to integrate and render Arbor with your engine/renderer. |

## Building

Include `arbor.h` in your project.
In **one** source file, define following to build implementation:

```c
#define ARBOR_IMPL
#include "arbor.h"
```

## License

This project is licensed under MIT License. See [LICENSE.md](LICENSE.md).
