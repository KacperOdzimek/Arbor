# Design Reference 
## Node

Arbor UI consists of *nodes* in code *arb_node*. 

```c
typedef struct arb_node {
    const arb_type* type;
    const uint32_t  flags;

    union {
        void*   data;
        size_t  data_offset;
    };
} arb_node;
```

Each node consists of *type* which determines how does node behave, *flags* allowing altering this behavior, and *data* which parametrise *type*.
With their childrens, node forms *UI tree*. Flags are bitfield, so multiple can be combined with ```|``` bit-or operator.

## Tree Definition

### Single childed nodes:

A arbor widget can be definied as flat nodes array:
<table>
  <tr>
    <th>Code</th>
    <th>Result</th>
  </tr>
  <tr>
    <td valign="top">
<pre><code class="language-c">arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height &(arb_box_data){
        .tint = ARB_HEX("#2f3d30")
    }),
    ARB_PADD(40),
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height &(arb_box_data){
        .tint = ARB_HEX("#2c4b2e"),
        .rounding = 16
    }),
    ARB_LAST
};</code></pre>
    </td>
    <td valign="top">
      <img src="design_reference_asset/nesting_example.png" width="300" alt="Nested UI">
    </td>
  </tr>
</table>

Single-child nodes interpret next element of array as own nested child.
``ARB_LAST`` defines end of chain, preventing reading out-of-bounds.

### Array childed nodes:

Array childed nodes, like rows, columns, overlays, except a array of nodes of ``arb_indirect_type`` type. This type of node sends interpreter to another array of nodes - those will be nested as a single container child. To avoid declaring a lot of separate arrays, ARB_ELEM macro can be used - it definies indirect node to local inline nodes array.

Example:

<table>
  <tr>
    <th>Code</th>
    <th>Result</th>
  </tr>
  <tr>
    <td valign="top">
<pre><code class="language-c">arb_node separate_array[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
        .tint = ARB_HEX("#49c554"), .rounding = 16
    }), ARB_LAST
};

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
        .tint = ARB_HEX("#323c32")
    }),
    ARB_PADD(40),
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
        .horizontal_align = 0.5, .spacing = (arb_length){0, 20, 1}}
    ),
    ARB_ELEM(
        ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
            .tint = ARB_HEX("#628865"), .rounding = 16
        }), ARB_LAST
    ),
    ARB_IDIR(separate_array),
    ARB_ELEM(
        ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
            .tint = ARB_HEX("#01c812"), .rounding = 16
        }), ARB_LAST
    ),
    ARB_LAST
}; </code></pre>
    </td>
    <td valign="top">
      <img src="design_reference_asset/array_nesting_example.png" width="300" alt="Nested UI">
    </td>
  </tr>
</table>

In example above, column is wrapped by background box and uniform 40px padding. The column contains, two boxes, inlined with ARB_ELEM and one, in separate array pointed by ARB_IDIR indirect node.

## Layout

By default node wraps on it's children. That is following widget:

```c
arb_node widget[] = {
    ARB_NODE(arb_box_type, arb_flag_none, &(arb_box_data){
        .tint = ARB_HEX("#ffffff")
    }),
    ARB_LAST
};
```

Would render with 0-size as box will wrap on nothing. 
This behavior can be altered with following flags:

```
arb_flag_ignore_min_width   // Min width  of this node is set to 0
arb_flag_ignore_min_height  // Min height of this node is set to 0
arb_flag_ignore_max_width   // Max width  of this node is set to inf
arb_flag_ignore_max_height  // Max height of this node is set to inf
```

Therefore following widget would render spanning entire screen:
```c
arb_node widget[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
        .tint = ARB_HEX("#ffffff")
    }),
    ARB_LAST
};
```

Flags are applied at respective *measure* passes.
Ingore min flags are usefull when used with clipboxes (otherwise clipbox would wrap on children, and not clip anything).

## Type

To learn on creating own types, see [Internal Reference](documentation/internal_reference.md). Here we will look at predefinied node types.
