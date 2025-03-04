import lark
import pygccxml.utils
import pygccxml.declarations
import pygccxml.parser
import sys
import os
import textwrap
import copy
import re
import enum
import io
import itertools
import collections
import logging
from typing import List, Dict, Set

VERBOSE = os.getenv("CONFGEN_VERBOSE", "0") == "1"

if not VERBOSE:
    pygccxml.utils.loggers.set_level(logging.ERROR)

def dprint(*args, **kwargs):
    if not VERBOSE:
        return
    print(*args, **kwargs)

def warn(*args, **kwargs):
    print(*args, **kwargs, file=sys.stderr)

if len(sys.argv) < 3:
    print("usage: {} <set of all input headers> <path to output source>".format(sys.argv[0]))

comment_expr = re.compile(r"^\s*//!cfg:(.*)$")

with open(os.path.join(os.path.dirname(__file__), "grammar.lark")) as f:
    comment_parser = lark.Lark(f.read(), start="comment")

collected_bare_paths = []
collected_bare_typedefs = {}
collected_loader_nodes = {}
# map of functiondecl --> (resultindex,locator)
collected_loader_properties = {}

# Entry actions:
# Called on the first thing matched by a node

# This is used to implement !size.
class UpdateSizeRef:
    def __init__(self, target: "ObjectLocator"):
        self.target = target

    def clone(self):
        return UpdateSizeRef(self.target.clone())

class UpdatePresenceRef:
    def __init__(self, target: "ObjectLocator"):
        self.target = target

    def clone(self):
        return UpdatePresenceRef(self.target.clone())

# A default argument for something, either on its own (for a variable), or an ARRAY_REF which is set to the position in an array an object is.
class DefaultArgType(enum.Enum):
    RAW_CPPVALUE = 0
    ARRAY_REF = 1

DefaultArg = collections.namedtuple("DefaultArg", ["kind", "value"])

def convert_default_args(default_args: List[DefaultArg], substitutions):
    return "{" + ", ".join(x.value if x.kind == DefaultArgType.RAW_CPPVALUE else substitutions[x.value] for x in default_args) + "}"

# Psuedo-action; used to indicate defaults and have them propagate properly before consolidating them somewhere into a big array.
class DoDefaultConstruct:
    def __init__(self, target: "ObjectLocator", defaultargs: List[DefaultArg]):
        self.target = target
        self.defaultargs = defaultargs

    def clone(self):
        return DoDefaultConstruct(self.target.clone(), self.defaultargs)

class EnsureLazyInit:
    def __init__(self, target: "ObjectLocator"):
        self.target = target

    def clone(self):
        return EnsureLazyInit(self.target.clone())

# A single "path node".
class BaseJsonPathNode:
    class DynName:
        def __init__(self, var: str):
            self.var = var

        def __repr__(self):
            return f"DynName[{self.var}]"

    NOT_ARRAY = -3
    ANY_ARRAY = -2
    SIZE_ARRAY = -4
    PRESENCE_ARRAY = -5
    DYNAMIC_ARRAY = -1

    # Can match any name -- used for variable subst.
    # If array_dynamic_target is set, it uses that for which to match
    ANY_NAME = type('_ANYNAME', (), {'__repr__': lambda x: "<ANY_NAME>"})()

    def __init__(self, name, array_index, array_dynamic_target=None):
        self.name = name # name or None for unnamed (like root)
        self.array_index = array_index # constant array index or constant above
        self.array_maximum = None # maximum value for array_index, or None for no maximum
        self.array_dynamic_target = array_dynamic_target # argument name for dynamic target (for loaders)
        self.parent_node: BaseJsonPathNode = None # reference to parent node

        self.entry_actions = [] # actions to run on entry into this node for the first time with unique array_index

    @property
    def is_root(self):
        return self.name is None and self.array_index == self.NOT_ARRAY

    @property
    def is_array(self):
        return self.array_index >= self.ANY_ARRAY

    @property
    def is_nonconst_name(self):
        return self.name == self.ANY_NAME or isinstance(self.name, BaseJsonPathNode.DynName) 

    @property
    def node_depth(self):
        if self.parent_node is None:
            return 0
        else:
            return 1 + self.parent_node.node_depth

    # copy attributes of this node onto another
    def clone_onto(self, into, include_parent=False):
        into.name = self.name
        into.array_index = self.array_index
        into.array_maximum = self.array_maximum
        into.array_dynamic_target = self.array_dynamic_target
        into.parent_node = self.parent_node if include_parent else None
        into.entry_actions = [x.clone() for x in self.entry_actions]

    def __str__(self):
        return str((self.name, self.array_index, self.entry_actions))

class ObjectLocator:
    def __init__(self, target_declaration: pygccxml.declarations.variable_t, array_indices: List[int], next_in_chain=None):
        self.target_declaration = target_declaration
        self.array_indices = array_indices
        self.next_in_chain = next_in_chain

    def append(self, n: "ObjectLocator"):
        nobj = self.clone()
        if nobj.next_in_chain is None:
            nobj.next_in_chain = n
        else:
            nobj.next_in_chain.append(n)
        return nobj

    def __eq__(self, other):
        if type(other) != type(self): return False
        return other.target_declaration == self.target_declaration and self.array_indices == other.array_indices and self.next_in_chain == other.next_in_chain

    def __hash__(self):
        return hash((self.target_declaration, tuple(self.array_indices), self.next_in_chain))

    def make_root(self):
        r = self.clone()
        r.next_in_chain = None
        return r

    def cut_at(self, pos):
        newv = self.clone()
        for j, i in enumerate(newv):
            if j == pos:
                i.next_in_chain = None
                return newv
        return newv

    @property
    def tip(self):
        if self.next_in_chain:
            return self.next_in_chain.tip
        else:
            return self

    @property
    def length(self):
        if self.next_in_chain is None:
            return 1
        else:
            return 1 + self.next_in_chain.length

    @property
    def target_type(self):
        return self.target_declaration.decl_type

    def __iter__(self):
        yield self
        if self.next_in_chain:
            yield from self.next_in_chain

    def clone(self):
        return ObjectLocator(self.target_declaration, self.array_indices[:], self.next_in_chain.clone() if self.next_in_chain is not None else None)

    def adjust_indices_by(self, amount: int):
        for j in range(len(self.array_indices)):
            self.array_indices[j] += amount
        if self.next_in_chain is not None:
            self.next_in_chain.adjust_indices_by(amount)

    def baseon(self, n: "ObjectLocator", with_adjustment=0):
        our_clone = self.clone()
        our_clone.adjust_indices_by(with_adjustment)
        return n.clone().append(our_clone)

    def __str__(self):
        buf = f"{self.target_declaration}@[{','.join(f'${i}' for i in self.array_indices)}]"
        if self.next_in_chain is not None:
            buf += f" -> {self.next_in_chain}"
        return buf

class ObjectLocatorDelegated(ObjectLocator):
    def __init__(self, target_declaration: pygccxml.declarations.calldef_t, bound_arg_indices: Dict[str, int]):
        self.target_declaration = target_declaration
        self.bound_arg_indices = bound_arg_indices
        self.next_in_chain = None

    def append(self, n):
        raise ValueError("can't append to delegate")

    def adjust_indices_by(self, amount: int):
        for j in self.bound_arg_indices:
            self.bound_arg_indices[j] += amount

    def clone(self):
        return ObjectLocatorDelegated(self.target_declaration, self.bound_arg_indices.copy())

    def __str__(self):
        return f"{self.target_declaration} {self.bound_arg_indices}"

    def __hash__(self):
        return hash((self.target_declaration, self.bound_arg_indices))

    def __eq__(self, other):
        if type(other) != type(self): return False
        return other.target_declaration == self.target_declaration and self.bound_arg_indices == other.bound_arg_indices

class ObjectLocatorGeneratorReturn(ObjectLocator):
    @property
    def target_type(self):
        return pygccxml.declarations.remove_reference(self.target_declaration.arguments[collected_loader_properties[self.target_declaration][0]].decl_type)

    def clone(self):
        return ObjectLocatorGeneratorReturn(self.target_declaration, self.array_indices[:], self.next_in_chain.clone() if self.next_in_chain is not None else None)

    def __str__(self):
        buf = f"({self.target_declaration}@[{','.join(f'${i}' for i in self.array_indices)}]).returnobj"
        if self.next_in_chain is not None:
            buf += f" -> {self.next_in_chain}"
        return buf

    def __hash__(self):
        return hash((self.target_declaration, tuple(self.array_indices), self.next_in_chain))

    def __eq__(self, other):
        if type(other) != type(self): return False
        return other.target_declaration == self.target_declaration and self.next_in_chain == other.next_in_chain and self.array_indices == other.array_indices
        
class DelegateJsonPathNode(BaseJsonPathNode):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.leaves = []

    def clone(self):
        new_node = DelegateJsonPathNode(self.name, self.array_index, self.array_dynamic_target)
        self.clone_onto(new_node)
        for i in self.leaves:
            new_i = i.clone()
            new_i.parent_node = new_node
            new_node.leaves.append(new_i)
        return new_node

    def _array_overlap(self, a, b):
        if a == BaseJsonPathNode.ANY_ARRAY:
            return True
        elif a == BaseJsonPathNode.NOT_ARRAY:
            return True
        else:
            if b >= 0:
                return a == b
            else:
                return self._array_overlap(b, a)

    def add_leaf(self, leaf: BaseJsonPathNode):
        # Resolve conflicts
        for j, my_leaf in enumerate(self.leaves):
            if (my_leaf.name == leaf.name or my_leaf.is_nonconst_name or leaf.is_nonconst_name) and self._array_overlap(my_leaf.array_index, leaf.array_index):
                if my_leaf.array_index != leaf.array_index:
                    raise ValueError("conflict between {} and {}'s arrays".format(my_leaf, leaf))
                if my_leaf.array_maximum != leaf.array_maximum and leaf.array_maximum is not None:
                    new_max = max(my_leaf.array_maximum, leaf.array_maximum) if my_leaf.array_maximum is not None else leaf.array_maximum
                    if my_leaf.array_maximum is not None:
                        warn("warn: conflicting maximums for {}, using {}".format(my_leaf, new_max))
                    my_leaf.array_maximum = new_max
                    leaf.array_maximum    = new_max
                if isinstance(leaf, DelegateJsonPathNode):
                    my_leaf.entry_actions.extend(leaf.entry_actions)
                    if not isinstance(my_leaf, DelegateJsonPathNode) and leaf.leaves:
                        if isinstance(my_leaf, ForwardDeclarationPathNode):
                            my_leaf.pending_leaves.extend(leaf.leaves)
                            break
                        else:
                            raise ValueError("conflict: {} and {} leaf-nonleaf".format(my_leaf, leaf))
                else:
                    if isinstance(my_leaf, DelegateJsonPathNode):
                        if my_leaf.leaves:
                            if isinstance(leaf, ForwardDeclarationPathNode):
                                leaf.pending_leaves.extend(my_leaf.leaves)
                            else:
                                raise ValueError("conflict: {} and {} leaf-nonleaf".format(my_leaf, leaf))
                        self.leaves[j] = leaf
                        self.leaves[j].entry_actions.extend(my_leaf.entry_actions)
                        break
                    else:
                        raise ValueError("conflict between {} and {}'s contents".format(my_leaf, leaf))
                # adopt leaves
                for sub_leaf in leaf.leaves:
                    my_leaf.add_leaf(sub_leaf)
                break
        else:
            self.leaves.append(leaf)
        leaf.parent_node = self

    def __str__(self):
        return "Delegate" + super().__str__() + "{" + ",".join(str(x) for x in self.leaves) + "}"

class StoreValuePathNode(BaseJsonPathNode):
    def __init__(self, name, array_index, array_dyn, target: ObjectLocator):
        super().__init__(name, array_index, array_dyn)

        self.target = target

    def __str__(self):
        return "Store" + super().__str__() + "{" + str(self.target) + "}"

    def clone(self):
        new_node = StoreValuePathNode(self.name, self.array_index, self.array_dynamic_target, self.target.clone())
        self.clone_onto(new_node)
        return new_node

class ForwardDeclarationPathNode(BaseJsonPathNode):
    def __init__(self, name, array_index, array_dyn, target: ObjectLocator, referencing_type: pygccxml.declarations.type_t):
        super().__init__(name, array_index, array_dyn)

        self.target = target
        self.referencing_type = referencing_type

        self.pending_leaves = []

    def __str__(self):
        return "ForwardStore" + super().__str__() + "{" + str(self.target) + "} to " + str(self.referencing_type)

    def clone(self):
        new_node = ForwardDeclarationPathNode(self.name, self.array_index, self.array_dynamic_target, self.target.clone(), self.referencing_type)
        self.clone_onto(new_node)
        return new_node

# parser config
gp, gn = pygccxml.utils.find_xml_generator()
xml_generator_config = pygccxml.parser.xml_generator_configuration_t(
    cflags="--std=c++20 -Wno-pragma-once-outside-header",
    xml_generator_path=gp,
    xml_generator=gn)

# The goal is to build a "node tree" that corresponds to ways the object can be parsed, and
# what to do with that parsed data.
#
# Data can work in one of three ways:
#   - stored into a variable
#   - delegated to a function
#     - that function can be either a Handler or a SubNode or an ObjectFiller or an ArrayFiller

LAZY_BEFORE_ARRAY = 2
LAZY_AFTER_ARRAY  = 1

def parse_gcc_array_string(declstring):
    ma = re.match(r"([a-zA-Z0-9<>_,: \t]+?)\s*((?:\[\d+\])+\s*)", declstring)
    if not ma:
        return declstring, []
    else:
        base = ma.group(1)
        tail = ma.group(2)
        section = tail.split("]")
        dimensions = [int(x[1:]) for x in section if x]
        return base, dimensions

def find_ns_parent(decl):
    while not isinstance(decl, pygccxml.declarations.namespace_t):
        decl = decl.parent
    return decl

def remove_lazy_template(orig_decl):
    decl = pygccxml.declarations.remove_declarated(orig_decl)
    if not pygccxml.declarations.is_class_declaration(decl) and not pygccxml.declarations.is_class(decl):
        return False, orig_decl
    if not pygccxml.declarations.templates.is_instantiation(decl.decl_string):
        return False, orig_decl
    name, args = pygccxml.declarations.templates.split(decl.decl_string)
    if name.startswith("::"):
        name = name[2:]
    if name == "config::lazy_t":
        base, arraydim = parse_gcc_array_string(args[0])
        if pygccxml.declarations.is_class(decl):
            base = pygccxml.declarations.remove_alias(decl.typedef("inner"))
            return True, base
        if base == "const char *":
            base = pygccxml.declarations.pointer_t(pygccxml.declarations.const_t(pygccxml.declarations.char_t()))
        else:
            try:
                base = global_namespace.decl(base)
            except pygccxml.declarations.runtime_errors.declaration_not_found_t:
                base = global_namespace.decl("::" + base)
        if isinstance(base, pygccxml.declarations.class_t):
            base = pygccxml.declarations.declarated_t(base)
        for dim in reversed(arraydim):
            base = pygccxml.declarations.array_t(base, dim)
        return True, base
    else:
        return False, orig_decl

def helper_array_dimensions_and_underlying_lazy(decl):
    outer_lazy, decl = remove_lazy_template(decl)
    dimensions = []
    while pygccxml.declarations.is_array(decl):
        dimensions.append(pygccxml.declarations.array_size(decl))
        decl = pygccxml.declarations.array_item_type(decl)
    inner_lazy, decl = remove_lazy_template(decl)
    if outer_lazy:
        lazy = LAZY_BEFORE_ARRAY
    elif inner_lazy:
        lazy = LAZY_AFTER_ARRAY
    else:
        lazy = 0
    return dimensions, decl, lazy

def find_all_tags(comment_data: lark.Tree, tag_type: str):
    for i in comment_data.children[2:]:
        if i.data == tag_type:
            yield i

class ValueTypePrimitive(enum.Enum):
    STRING = 0
    RAW_STRING = 4
    INTEGER = 1
    FLOATING_POINT = 2
    BOOLEAN = 3

class ValueTypeCustom:
    def __init__(self, referenced_type: pygccxml.declarations.class_t):
        self.referenced_type = referenced_type

class ValueTypeEnum:
    def __init__(self, referenced_enum: pygccxml.declarations.enumeration_t):
        self.referenced_type = referenced_enum

class ValueTypeArray:
    def __init__(self, inner, dimensions):
        self.inner = inner
        self.dimensions = dimensions

class ValueTypeLazyPointer:
    def __init__(self, subtype):
        self.subtype = subtype

declared_enums: Set[pygccxml.declarations.enumeration_t] = set()

def value_type_for(decl):
    global declared_enums
    dimension, subdecl, is_lazy = helper_array_dimensions_and_underlying_lazy(decl)
    if pygccxml.declarations.is_same(pygccxml.declarations.remove_cv(pygccxml.declarations.remove_pointer(subdecl)), pygccxml.declarations.char_t()):
        inner = ValueTypePrimitive.RAW_STRING
    elif pygccxml.declarations.is_same(pygccxml.declarations.remove_cv(subdecl), string_t):
        inner = ValueTypePrimitive.STRING
    elif pygccxml.declarations.is_bool(subdecl):
        inner = ValueTypePrimitive.BOOLEAN
    elif pygccxml.declarations.is_floating_point(subdecl):
        inner = ValueTypePrimitive.FLOATING_POINT
    elif pygccxml.declarations.is_integral(subdecl):
        inner = ValueTypePrimitive.INTEGER
    elif pygccxml.declarations.is_enum(subdecl):
        inner = ValueTypeEnum(subdecl)
        declared_enums.add(subdecl)
    elif pygccxml.declarations.is_class(subdecl):
        inner = ValueTypeCustom(subdecl)
    else:
        raise NotImplementedError(subdecl)
    if is_lazy == LAZY_AFTER_ARRAY:
        inner = ValueTypeLazyPointer(inner)
    if dimension:
        inner = ValueTypeArray(inner, dimension)
    if is_lazy == LAZY_BEFORE_ARRAY:
        return ValueTypeLazyPointer(inner)
    else:
        return inner

def get_default_default(t):
    # get the default default (oh yes)
    if isinstance(t, ValueTypeArray):
        t = t.inner
    if isinstance(t, ValueTypeCustom) or t == ValueTypePrimitive.STRING:
        return [] # default construct
    if t == ValueTypePrimitive.RAW_STRING:
        return [DefaultArg(DefaultArgType.RAW_CPPVALUE, "nullptr")]
    elif t == ValueTypePrimitive.INTEGER:
        return [DefaultArg(DefaultArgType.RAW_CPPVALUE, "0")]
    elif t == ValueTypePrimitive.FLOATING_POINT:
        return [DefaultArg(DefaultArgType.RAW_CPPVALUE, "0.")]
    elif t == ValueTypePrimitive.BOOLEAN:
        return [DefaultArg(DefaultArgType.RAW_CPPVALUE, "false")]
    elif isinstance(t, ValueTypeEnum):
        raise ValueError("must provide explicit default for enum type")
    else:
        raise NotImplementedError(t)

def generate_single_entry_for_declaration(decl: pygccxml.declarations.declaration_t, comment_data: lark.Tree):
    # First, check if the types agree
    intent = comment_data.children[0].value
    if intent == "holds" and not isinstance(decl, pygccxml.declarations.variable_t):
        raise ValueError("invalid type: holds should be annotating a variable.")
    elif intent in ["receives", "loads"] and not isinstance(decl, pygccxml.declarations.calldef_t):
        raise ValueError("invalid type: receives/loads should be annotating a function.")

    if intent not in ["holds", "receives", "loads"]:
        raise NotImplementedError(intent)

    # Check if this is a free function / nonmember
    free = True

    if isinstance(decl, pygccxml.declarations.variable_t) and isinstance(decl.parent, pygccxml.declarations.class_t) and decl.type_qualifiers.has_static == False:
        free = False
    elif isinstance(decl, pygccxml.declarations.member_function_t) and not decl.has_static:
        free = False

    if not free and intent == "loads":
        raise ValueError("invalid type: a loader routine cannot be a class member")

    if not free:
        parent_t = decl.parent
        if parent_t not in collected_bare_typedefs:
            collected_bare_typedefs[parent_t] = []
        target_list = collected_bare_typedefs[parent_t]
    elif intent == "loads":
        collected_loader_nodes[decl] = []
        collected_loader_properties[decl] = [None,None]
        for j, arg in enumerate(decl.arguments):
            if arg.name == "value":
                collected_loader_properties[decl][0] = j
        if collected_loader_properties[decl][0] is None:
            raise ValueError("loader routine must have a value argument which is a reference type")
        target_list = collected_loader_nodes[decl]
    else:
        target_list = collected_bare_paths

    # Begin building names

    names = [(None, BaseJsonPathNode.NOT_ARRAY, None)]
    variable_map = {}

    forbidden_vars = set("value")
    if intent == "loads":
        forbidden_vars.add("curnode")
        forbidden_vars.add("returnobj")
        forbidden_vars.add("v")
        forbidden_vars.add("stack")
        forbidden_vars.add("stack_ptr")

    for j, leaf in enumerate(comment_data.children[1].children):
        dyntarget = None
        if leaf.data == "path_element":
            name = leaf.children[0].value
            array = BaseJsonPathNode.NOT_ARRAY
        elif leaf.data in ("named_array_element", "anon_array_element"):
            name = leaf.children[0].value if leaf.data == "named_array_element" else None
            idx  = leaf.children[1]       if leaf.data == "named_array_element" else leaf.children[0]
            if not isinstance(idx, lark.Token) and idx.data == "variable":
                array = BaseJsonPathNode.ANY_ARRAY
                variable_map[idx.children[0].value] = j + 1
                if idx.children[0].value in forbidden_vars:
                    raise ValueError("illegal variable name")
                if intent == "loads" and any(x.name == idx.children[0].value for x in decl.arguments):
                    array = BaseJsonPathNode.DYNAMIC_ARRAY
                    dyntarget = idx.children[0].value
                    del variable_map[idx.children[0].value]
            else:
                array = int(idx.value)
        elif leaf.data == "wildcard_element":
            if intent == "holds":
                raise ValueError("invalid variable: only valid for functions")
            name = BaseJsonPathNode.ANY_NAME
            array = BaseJsonPathNode.NOT_ARRAY
            varname = leaf.children[0].children[0].value 
            if varname in forbidden_vars:
                raise ValueError("illegal variable name")
            if intent == "loads" and any(x.name == varname for x in decl.arguments):
                name = BaseJsonPathNode.DynName(varname)
            elif intent == "loads":
                raise ValueError("dynamic name not present in function args")
            else:
                variable_map[varname] = j + 1
        elif leaf.data in ["special_size_element", "special_presence_element", "anon_size_element"]:
            name = leaf.children[0].value if leaf.data != "anon_size_element" else None
            array = BaseJsonPathNode.SIZE_ARRAY if leaf.data != "special_presence_element" else BaseJsonPathNode.PRESENCE_ARRAY
        else:
            raise NotImplementedError(leaf.data)
        names.append([name, array, dyntarget])

    array_maximum_map = {}
    is_custom_type = False
    dimensions = []
    underlying = None
    is_lazy = False

    # Populate array information (and custom type)
    if intent == "holds" or intent == "loads":
        the_type = decl.decl_type if intent == "holds" else pygccxml.declarations.remove_reference(decl.arguments[collected_loader_properties[decl][0]].decl_type)
        dimensions, underlying, is_lazy = helper_array_dimensions_and_underlying_lazy(the_type)
        is_custom_type = isinstance(underlying, pygccxml.declarations.declarated_t) and isinstance(underlying.declaration, pygccxml.declarations.class_t) and not \
            pygccxml.declarations.is_same(string_t, underlying)

        if intent == "loads" and dimensions and not is_lazy:
            raise ValueError("cannot have loader returning array; return an object proxy instead")

        # Propagate the array information (max size on each array matcher, hook up i/j/k/etc. to array_indices

        for j, letter in enumerate("ijkl"[:len(dimensions)]):
            array_maximum_map[letter] = dimensions[j]
            if letter not in variable_map:
                # Try to guess where it should go, based on names
                if names[-1][1] == BaseJsonPathNode.NOT_ARRAY:
                    # Map into end
                    names[-1][1] = BaseJsonPathNode.ANY_ARRAY
                    variable_map[letter] = len(names) - 1
                elif names[-1][1] in [BaseJsonPathNode.SIZE_ARRAY, BaseJsonPathNode.PRESENCE_ARRAY]:
                    raise NotImplementedError("unknown how to handle auto-array for size_array")
                else:
                    # Add new anonymous entry
                    variable_map[letter] = len(names)
                    names.append([BaseJsonPathNode.ANY_NAME, BaseJsonPathNode.ANY_ARRAY, None])

    for custom_maximum in find_all_tags(comment_data, "explicit_array_max"):
        requested = int(custom_maximum.children[1].value)
        for_var   = custom_maximum.children[0].children[0].value
        if for_var not in variable_map:
            raise ValueError("unknown array var {}".format(for_var))
        if array_maximum_map.get(for_var, requested) < requested:
            warn("warn: requested max is larger than autodetected max {} < {} ({})".format(array_maximum_map[for_var], requested, for_var))
        array_maximum_map[for_var] = requested

    def common_make_node(j, kind, *args):
        name, array_index, array_dyntarget = names[j]
        new_node = kind(name, array_index, array_dyntarget, *args)
        # Try and propagate array maximums
        for entry, location in variable_map.items():
            if entry in array_maximum_map and location == j:
                new_node.array_maximum = array_maximum_map[entry]
        return new_node

    # Generate intermediate steps
    root = None
    end = None
    for j in range(len(names)-1):
        new_node = common_make_node(j, DelegateJsonPathNode)
        if j == 0:
            root = end = new_node
        else:
            end.add_leaf(new_node)
            end = new_node

    # Generate final node
    if intent == "holds":
        # Create locator
        locator = ObjectLocator(decl, [variable_map["ijkl"[j]] for j in range(len(dimensions))])
    elif intent == "receives":
        locator = ObjectLocatorDelegated(decl, variable_map)
    else:
        locator = ObjectLocatorGeneratorReturn(decl, [variable_map["ijkl"[j]] for j in range(len(dimensions))])

    if is_custom_type:
        final_node = common_make_node(len(names)-1, ForwardDeclarationPathNode, locator, underlying.declaration)
    else:
        # Specifically handle sizes here
        if names[-1][1] == BaseJsonPathNode.SIZE_ARRAY:
            names[-1][1] = BaseJsonPathNode.ANY_ARRAY
            final_node = common_make_node(len(names)-1, DelegateJsonPathNode)
            final_node.entry_actions.append(UpdateSizeRef(locator))
        elif names[-1][1] == BaseJsonPathNode.PRESENCE_ARRAY:
            names[-1][1] = BaseJsonPathNode.NOT_ARRAY
            final_node = common_make_node(len(names)-1, DelegateJsonPathNode)
            final_node.entry_actions.append(UpdatePresenceRef(locator))
        else:
            final_node = common_make_node(len(names)-1, StoreValuePathNode, locator)

    defaultargs = None

    # Setup default value if applicable
    try:
        v = next(find_all_tags(comment_data, "default_tag"))
        if intent == "receives":
            raise ValueError("receives cannot have defaults")

        if is_lazy:
            raise ValueError("cannot have explicit default for lazy")
        
        defaultargs = []
        for i in v.children:
            if i.data in ["float_or_int", "string", "named_constant"]:
                defaultargs.append(DefaultArg(DefaultArgType.RAW_CPPVALUE, i.children[0].value))
            else:
                defaultargs.append(DefaultArg(DefaultArgType.ARRAY_REF, "ijkl".index(i.children[0].children[0].value)))
    except StopIteration:
        if intent == "holds" and free and not is_lazy:
            # Generate default default
            defaultargs = get_default_default(value_type_for(decl.decl_type))

    if intent == "holds" and value_type_for(decl.decl_type) == ValueTypePrimitive.RAW_STRING:
        warn(f"warn: using a raw char * for {decl} will cause the sim to leak memory; try using a config::string_t instead")

    if intent != "receives" and defaultargs is not None:
        final_node.entry_actions.append(DoDefaultConstruct(locator, defaultargs))

    if is_lazy:
        final_node.entry_actions.insert(0, EnsureLazyInit(locator))

    if intent == "loads":
        collected_loader_properties[decl][1] = locator

    end.add_leaf(final_node)
    target_list.append(root)

all_declarations = []
global_namespace = None
string_t = None

def parse_file(fnames):
    global all_declarations, global_namespace, string_t
    all_declarations = pygccxml.parser.parse(fnames, config=xml_generator_config, compilation_mode=pygccxml.parser.COMPILATION_MODE.ALL_AT_ONCE)
    comments_in_file = {}
    fnames_to_find = fnames[:] 
    fnames_found = set()
    while fnames_to_find:
        fname = fnames_to_find.pop()
        with open(fname, "r") as f:
            ln = 0
            for line in f:
                ln += 1
                m = comment_expr.match(line)
                if m:
                    data = m.group(1).strip()
                    if data.startswith("include "):
                        path = data[len("include "):]
                        if path not in fnames_to_find and path not in fnames_found:
                            if not os.path.isabs(path):
                                path = os.path.abspath(os.path.join(os.path.dirname(fname), path))
                            fnames_to_find.append(path)
                    else:
                        comments_in_file[(fname, ln)] = comment_parser.parse(data)
        fnames_found.add(fname)
    
    # Find all declarations that are either calldefs or variables
    broad_matcher = pygccxml.declarations.calldef_matcher() | pygccxml.declarations.variable_matcher()
    all_things = pygccxml.declarations.matcher.find(broad_matcher, all_declarations)
    global_namespace = pygccxml.declarations.get_global_namespace(all_declarations)
    string_t = global_namespace.namespace("config").class_("string_t")

    # Go through each one, and try to find ones with matching comments
    for thing in all_things:
        try:
            comment_should_be = (thing.location.file_name, thing.location.line - 1)
        except:
            continue
        if comment_should_be in comments_in_file:
            generate_single_entry_for_declaration(thing, comments_in_file[comment_should_be])

# STAGE 1: load all files
parse_file([os.path.abspath(x) for x in sys.argv[1:-1]])

def visit_all_subnodes_with(node, functor, child_first=True):
    if not child_first: functor(node)
    if isinstance(node, DelegateJsonPathNode):
        for j in node.leaves:
            visit_all_subnodes_with(j, functor, child_first)
    if child_first: functor(node)

def visit_all_forward_decls_with(node, functor):
    def subfunctor(node):
        if isinstance(node, ForwardDeclarationPathNode):
            functor(node)

    visit_all_subnodes_with(node, subfunctor)

# STAGE 2: begin processing all typedefs
def find_all_referenced_typedefs():
    referenced = set()

    marked = False
    def visit(node):
        nonlocal marked
        marked = True

    for i in collected_bare_typedefs:
        marked = False
        for j in collected_bare_typedefs[i]:
            visit_all_forward_decls_with(j, visit)
        if marked:
            referenced.add(i)

    return referenced

def merge_path_list(pathlist):
    new_root = DelegateJsonPathNode(None, BaseJsonPathNode.NOT_ARRAY)

    for subpath in pathlist:
        assert(isinstance(subpath, DelegateJsonPathNode) and subpath.is_root)
        for firstleaf in subpath.leaves:
            new_root.add_leaf(firstleaf)

    return new_root

def forward_typedef_into_others(decltype):
    # Prepare a full tree
    replacement_tree = merge_path_list(collected_bare_typedefs.pop(decltype))

    dprint("merged tree for", decltype)
    dump_node_tree(replacement_tree)

    def visit(node: ForwardDeclarationPathNode):
        if node.referencing_type != decltype:
            return

        # Ok, we need to replace this node
        node.parent_node.leaves.remove(node)

        # Construct a new node
        private_tree = replacement_tree.clone()
        assert isinstance(private_tree, DelegateJsonPathNode)

        # Update all subnodes
        def fixup(subnode: BaseJsonPathNode):
            # Update all size refs
            for action in subnode.entry_actions:
                if hasattr(action, "target"):
                    action.target = action.target.baseon(node.target, node.node_depth)
                # update default constructs. these are extracted later
                if isinstance(action, DoDefaultConstruct):
                    for i in action.defaultargs:
                        if i.kind == DefaultArgType.ARRAY_REF:
                            i.value += node.node_depth

            # Update all stores
            if isinstance(subnode, StoreValuePathNode) or isinstance(subnode, ForwardDeclarationPathNode):
                subnode.target = subnode.target.baseon(node.target, node.node_depth)

        # Fix up all subnodes in private tree
        visit_all_subnodes_with(private_tree, fixup)
        node.clone_onto(private_tree, include_parent=True)
        for i in node.pending_leaves:
            private_tree.add_leaf(i)
        node.parent_node.leaves.append(private_tree)

    for i in itertools.chain(collected_bare_paths, *collected_bare_typedefs.values(), *collected_loader_nodes.values()):
        visit_all_forward_decls_with(i, visit)

def dump_node_tree(node: BaseJsonPathNode, indentdepth=0):
    def lprint(*args):
        dprint(" "*indentdepth + str(args[0]), *args[1:])

    if node.is_root:
        lprint("- (root)")
    elif node.is_array:
        array_idx = str(node.array_index)
        if node.array_index == BaseJsonPathNode.ANY_ARRAY:
            array_idx = '0..' + (str(node.array_maximum) if node.array_maximum is not None else '')
        elif node.array_index == BaseJsonPathNode.DYNAMIC_ARRAY:
            array_idx = f"${node.array_dynamic_target}"
            if node.array_maximum is not None:
                array_idx += f"<0..{node.array_maximum}>"
        lprint(f"- {node.name}[{array_idx}]")
    else:
        lprint(f"- {node.name}")
    
    indentdepth += 2

    for action in node.entry_actions:
        if isinstance(action, UpdateSizeRef):
            lprint(f"on entry update sizeof", action.target)
        elif isinstance(action, UpdatePresenceRef):
            lprint(f"on entry mark presence", action.target)
        elif isinstance(action, DoDefaultConstruct):
            lprint(f"mark default construct of", action.target, "with [", *action.defaultargs, "]")
        elif isinstance(action, EnsureLazyInit):
            lprint(f"ensure", action.target, "is allocated")
        else:
            lprint(f"unknown action")

    if isinstance(node, DelegateJsonPathNode):
        for subnode in node.leaves:
            dump_node_tree(subnode, indentdepth + 2)
    elif isinstance(node, StoreValuePathNode):
        lprint("store to", node.target)
    elif isinstance(node, ForwardDeclarationPathNode):
        lprint("store to", node.target, "with forward declarations from", node.referencing_type)
    else:
        lprint("unknown type")

for j, i in collected_bare_typedefs.items():
    dprint("for", j)
    for k in i:
        dump_node_tree(k)
for j, i in collected_loader_nodes.items():
    dprint("for", j)
    for k in i:
        dump_node_tree(k)
dprint("global")
for i in collected_bare_paths:
    dump_node_tree(i)

while True:
    all_typedefs_known = set(collected_bare_typedefs.keys())
    blocked_to_process = find_all_referenced_typedefs()

    if not blocked_to_process <= all_typedefs_known:
        raise ValueError("unknown types: {}".format([str(x) for x in blocked_to_process - all_typedefs_known]))

    process_this_cycle = all_typedefs_known - blocked_to_process

    if len(process_this_cycle) == 0:
        break

    dprint("cycle", *(str(x) for x in process_this_cycle))
    dprint("blocked", *(str(x) for x in blocked_to_process))

    for i in process_this_cycle:
        forward_typedef_into_others(i)

# finally, merge to get final tree
options_tree = merge_path_list(collected_bare_paths)
for i in collected_loader_nodes.keys():
    collected_loader_nodes[i] = merge_path_list(collected_loader_nodes[i])

dprint("Collected configuration tree")
dump_node_tree(options_tree)
for j, i in collected_loader_nodes.items():
    dprint("for", j)
    dump_node_tree(i)

# Collect all defaults into one place
declared_variables: Dict[ObjectLocator, List[DefaultArg]] = {}

def extract_variables(of):
    removed = []
    ensures = []
    for i in of.entry_actions:
        if isinstance(i, DoDefaultConstruct):
            removed.append(i)
        if isinstance(i, EnsureLazyInit):
            ensures.append(i)
    for i in removed:
        of.entry_actions.remove(i)
        declared_variables[i.target] = i.defaultargs
    ensures.sort(key=lambda x: x.target.length, reverse=True)
    for i in ensures:
        of.entry_actions.remove(i)
        of.entry_actions.insert(0, i)
        declared_variables[i.target] = []

visit_all_subnodes_with(options_tree, extract_variables)
for i in collected_loader_nodes.values():
    visit_all_subnodes_with(i, extract_variables)

class Outputter:
    SHIFT_WIDTH = 4

    def __init__(self, indent=0, target=None):
        if target:
            self.result = target
        else:
            self.result = io.StringIO()
        self.indent = indent

    def __enter__(self):
        return Outputter(self.indent + Outputter.SHIFT_WIDTH, self.result)

    def __exit__(self, *args, **kwargs):
        pass

    def add(self, *args, **kwargs):
        self.result.write(" " * self.indent)
        print(*args, **kwargs, file=self.result)

    def value(self):
        return self.result.getvalue()

    def __iadd__(self, tgt):
        self.result.write(textwrap.indent(tgt, " "*self.indent))
        return self

# MAIN CODE GENERATION

def generate_header(output):
    output.add("// USED HEADERS")
    output.add('#include "json.h"')
    output.add("#include <string.h>")
    output.add("#include <stdint.h>")
    output.add("#include <stddef.h>")
    output.add("#include <stdlib.h>")
    output.add("#include <memory>")
    output.add("#include <esp_log.h>")
    output.add("#include <esp8266/eagle_soc.h>")
    output.add()
    output.add("// CFG HEADERS (for variable decls)")
    for i in sys.argv[1:-1]:
        output.add('#include "{}"'.format(os.path.abspath(i)))
    output.add()
    output.add('#pragma GCC diagnostic ignored "-Wunused-label"')
    output.add("const static char * const TAG = \"configparse\";")

def generate_variable_declarations(output):
    output.add("// VARIABLE DECLARATIONS (from other files)")
    output.add()
    for variable, generator in declared_variables.items():
        if variable.next_in_chain or isinstance(variable, ObjectLocatorGeneratorReturn):
            continue
        dimensions, underlying, is_lazy = helper_array_dimensions_and_underlying_lazy(variable.target_type)
        barename = variable.target_declaration.decl_string
        if barename.startswith("::"):
            barename = barename[2:]
        dimstr = ""
        # Generate array dimensions
        if dimensions:
            dimstr = ''.join(f"[{x}]" for x in dimensions)
        line = {
                0: f"{underlying} {barename}",
                LAZY_BEFORE_ARRAY: f"config::lazy_t<{underlying} {dimstr}> {barename}",
                LAZY_AFTER_ARRAY: f"config::lazy_t<{underlying}> {barename}{dimstr}",
        }[is_lazy]
        if not is_lazy:
            line += dimstr

            # Generate defaults
            if dimensions:
                line += ' = ';
                last = None
                for ijkl_values in itertools.product(
                    *(range(x) for x in dimensions)
                ):
                    close_braces = 0
                    open_braces = 0
                    for j in range(len(ijkl_values)-1, -1, -1):
                        if ijkl_values[j] == 0:
                            open_braces += 1
                        else:
                            break
                    for j in range(len(ijkl_values)-1, -1, -1):
                        if ijkl_values[j] == dimensions[j]-1:
                            close_braces += 1
                        else:
                            break

                    last = ijkl_values[:]
                    line += "{" * open_braces
                    line += convert_default_args(generator, [str(x) for x in ijkl_values])
                    line += "}" * close_braces
                    if any(ijkl_values[x] != dimensions[x]-1 for x in range(len(dimensions))):
                        line += ", "
            else:
                line += convert_default_args(generator, [])

        output.add(line + ";")

def generate_str_to_enum_funcs(output):
    output.add("// STRING TO ENUM LOOKUPS")
    output.add("template<typename T>")
    output.add("T lookup_enum(const char* v, T default_value) {")
    with output as defdef:
        defdef.add('ESP_LOGW(TAG, "Unknown enum type, assuming default");')
        defdef.add("return default_value;")
    output.add("}")
    for enum in declared_enums:
        enum = pygccxml.declarations.remove_declarated(enum)
        output.add()
        output.add("template<>")
        output.add(f"{enum.decl_string} lookup_enum<{enum.decl_string}>(const char *v, {enum.decl_string} default_value) {{")
        with output as lookupbody:
            made_if = False
            for i, _ in enum.values:
                compare = "else if" if made_if else "if"
                compare += f" (strcasecmp(v, \"{i}\") == 0) return {enum.decl_string}::{i};"
                made_if = True
                lookupbody.add(compare)
            lookupbody.add(f"else {{ ESP_LOGW(TAG, \"Unknown enum value %s for {enum.name}\", v); return default_value; }}")
        output.add("}")

subparser_used_counter = {}

def generate_subparser_state_name(locator: ObjectLocatorGeneratorReturn):
    global subparser_used_names
    
    if locator in subparser_used_counter:
        return subparser_used_counter[locator]

    name = f"subcstate_{len(subparser_used_counter)}"
    subparser_used_counter[locator] = name
    return name

def generate_nodeenum_declaration(output, tree, structname="cfgstate", docs="STATE ENUM"):
    node_to_symbol_map = {}

    output.add(f"// {docs}")
    output.add(f"enum struct {structname} : uint16_t {{")
    with output as inner:
        def add_to_symbol_map(node: BaseJsonPathNode):
            if not isinstance(node, DelegateJsonPathNode):
                return

            if node.is_root:
                name = "R"
            else:
                name = node_to_symbol_map[node.parent_node] + "__"
                if node.name == BaseJsonPathNode.ANY_NAME:
                    name += "ANY"
                elif isinstance(node.name, BaseJsonPathNode.DynName):
                    name += "D_" + node.name.var.upper()
                elif node.name is None:
                    name += "ANON"
                else:
                    name += node.name.upper()
                if node.array_index == BaseJsonPathNode.ANY_ARRAY:
                    name += "_ALL"
                elif node.array_index == BaseJsonPathNode.DYNAMIC_ARRAY:
                    name += "_D_" + node.array_dynamic_target.upper()
                elif node.array_index != BaseJsonPathNode.NOT_ARRAY:
                    name += f"_{node.array_index}"
                while name in node_to_symbol_map.values():
                    name += "_"

            node_to_symbol_map[node] = name
            inner.add(name + ",")

        visit_all_subnodes_with(tree, add_to_symbol_map, False)
    output.add("};")
    return node_to_symbol_map

def generate_redefault_function(output):
    output.add("// EXPLICIT DEFAULT")
    output.add("void clear_config() {")
    with output as inner:
        # Sort all defaultable values by length.
        varbs = list(declared_variables.keys())
        varbs.sort(key=lambda x: x.length)

        for variable_chain in varbs:
            if isinstance(variable_chain, ObjectLocatorGeneratorReturn):
                continue
            # prepare the output line
            lineoutputter = inner
            linechain = ""

            if any(
                helper_array_dimensions_and_underlying_lazy(v.target_type)[2] and k != variable_chain.length - 1
                for k, v in enumerate(variable_chain)
            ):
                continue

            # Create for loops and line access
            for k, variable in enumerate(variable_chain):
                dimensions, underlying, is_lazy = helper_array_dimensions_and_underlying_lazy(variable.target_type)

                if is_lazy == LAZY_BEFORE_ARRAY:
                    dimensions = []

                # Check if we need to prefix this with a for loop
                for j, dimension in enumerate(dimensions):
                    letter = "ijkl"[j] + str(k)
                    lineoutputter.add(f"for (size_t {letter} = 0; {letter} < {dimension}; ++{letter})")
                    lineoutputter = lineoutputter.__enter__()

                # Generate entry in chain
                if linechain:
                    linechain += "."
                linechain += variable.target_declaration.decl_string;
                if dimensions:
                    for i in "ijkl"[:len(dimensions)]:
                        linechain += f"[{i}{k}]"

                if is_lazy:
                    variable_chain = variable_chain.cut_at(k)
                    break

            variable = variable_chain.tip
            dimensions, underlying, is_lazy = helper_array_dimensions_and_underlying_lazy(variable.target_type)
            vartype = value_type_for(variable.target_type)

            # Ignore array definition
            if isinstance(vartype, ValueTypeArray):
                vartype = vartype.inner

            needs_close = False

            # Is this a custom object? use placement delete + placement new
            if isinstance(vartype, (ValueTypeCustom, ValueTypeLazyPointer)):
                lineoutputter.add(f"{{ std::destroy_at(&{linechain}); ")
                linechain = f"new (&{linechain}) std::remove_reference_t<decltype({linechain})>"
                needs_close = True
            elif vartype != ValueTypePrimitive.RAW_STRING:
                linechain += " ="
            else:
                lineoutputter.add(f"{{ if (IS_DRAM({linechain}) || IS_IRAM({linechain})) free(const_cast<char *>({linechain}));")
                linechain += " ="
                needs_close = True

            # Create args
            linechain += " " + convert_default_args(declared_variables[variable_chain], [
                "ijkl"[x] + str(variable_chain.length-1) for x in range(len(dimensions))
            ]) + ";"
            if needs_close:
                linechain += " }"
            lineoutputter.add(linechain)

    output.add("}")

def assignment_separator_for(locator):
    if isinstance(locator, ObjectLocatorGeneratorReturn):
        return "."
    else:
        return "->" if pygccxml.declarations.is_pointer(locator.target_type) else "."

def generate_assignment_path_for(locator: ObjectLocator, point_to_lazy_only=False):
    path = ""
    segments = list(locator)
    for j, i in enumerate(segments):
        if path:
            path += assignment_separator_for(segments[j-1])
        if isinstance(i, ObjectLocatorDelegated):
            path += pygccxml.declarations.full_name(i.target_declaration)
        elif isinstance(i, ObjectLocatorGeneratorReturn):
            path += "returnobj"
        else:
            path += i.target_declaration.decl_string
        # Add array refs
        if not isinstance(i, ObjectLocatorDelegated):
            _, __, is_lazy = helper_array_dimensions_and_underlying_lazy(i.target_type)
            if is_lazy == LAZY_BEFORE_ARRAY and not point_to_lazy_only:
                path = f"(*{path})"
            if not (is_lazy == LAZY_BEFORE_ARRAY and point_to_lazy_only):
                for k in i.array_indices:
                    path += f"[stack[{k}]->index]"
            if is_lazy == LAZY_AFTER_ARRAY and not point_to_lazy_only:
                path = f"(*{path})"
    return path

def generate_entry_action(output, action, node):
    if isinstance(action, UpdateSizeRef):
        output.add(generate_assignment_statement_for(action.target, f"stack[{node.node_depth}]->index+1"))
    elif isinstance(action, UpdatePresenceRef):
        output.add(generate_assignment_statement_for(action.target, "1"))
    elif isinstance(action, EnsureLazyInit):
        # Check lazy type
        _, underlying, lazy_kind = helper_array_dimensions_and_underlying_lazy(action.target.tip.target_type)
        if lazy_kind == LAZY_BEFORE_ARRAY:
            output.add(f"if (!{generate_assignment_path_for(action.target, point_to_lazy_only=True)}) {generate_assignment_path_for(action.target, point_to_lazy_only=True)} = new {action.target.tip.target_type.decl_string}::inner {{}};")
        else:
            output.add(f"if (!{generate_assignment_path_for(action.target, point_to_lazy_only=True)}) {generate_assignment_path_for(action.target, point_to_lazy_only=True)} = new {underlying.decl_string} {{}};")
    else:
        raise NotImplementedError(action)

def as_c_string(value: str):
    result = ""
    bytes_value = value.encode('utf-8')
    for i in bytes_value:
        if chr(i) in ["\\", '"']:
            result += "\\" + i
        elif not (32 <= i < 127):
            result += "\\x{:02x}".format(i)
        else:
            result += chr(i)
    return '"' + result + '"'

def get_stored_value_and_type_for(decl, access_string="{}", duplicate=True):
    vtype = value_type_for(decl)
    _, underlying, __ = helper_array_dimensions_and_underlying_lazy(decl)
    if isinstance(vtype, ValueTypeLazyPointer):
        vtype = vtype.subtype
    if isinstance(vtype, ValueTypeArray):
        vtype = vtype.inner
    if isinstance(vtype, ValueTypeCustom):
        raise ValueError("cannot store directly to custom type")
    if vtype == ValueTypePrimitive.RAW_STRING:
        return vtype, "strdup(v.str_val)" if duplicate else "v.str_val"
    elif vtype == ValueTypePrimitive.STRING:
        return ValueTypePrimitive.RAW_STRING, "{config::string_t::heap_tag{}, strdup(v.str_val)}"
    elif vtype == ValueTypePrimitive.BOOLEAN:
        return vtype, "v.bool_val"
    elif vtype == ValueTypePrimitive.FLOATING_POINT:
        return vtype, "v.as_number()"
    elif vtype == ValueTypePrimitive.INTEGER:
        return vtype, "v.int_val"
    elif isinstance(vtype, ValueTypeEnum):
        return ValueTypePrimitive.RAW_STRING, f"config::lookup_enum<{vtype.referenced_type}>(v.str_val, {access_string})"
    else:
        raise NotImplementedError(vtype)

def generate_assignment_statement_for(locator: ObjectLocator, value: str):
    line = generate_assignment_path_for(locator)
    tip = locator.tip
    if isinstance(tip, ObjectLocatorDelegated):
        line += "("
        for argument in tip.target_declaration.arguments:
            if line[-1] != "(": line += ", "
            if argument.name == "value":
                line += value
            elif argument.name in tip.bound_arg_indices:
                # Is this an array index?
                if pygccxml.declarations.is_integral(argument.decl_type):
                    line += f"stack[{tip.bound_arg_indices[argument.name]}]->index"
                else:
                    line += f"stack[{tip.bound_arg_indices[argument.name]}]->name"
            else:
                raise ValueError(f"unknown what to do with argument {argument} in {tip.target_declaration} (have you named the target value argument 'value'?)")
        line += ");"
    else:
        line += " = " + value + ";"
    return line

def generate_store_code(output, node: StoreValuePathNode):
    # TODO: generate type verification code
    tip = node.target.tip
    tip_decl = None
    if isinstance(tip, ObjectLocatorDelegated):
        for argument in tip.target_declaration.arguments:
            if argument.name == "value":
                tip_decl = argument.decl_type
    else:
        tip_decl = tip.target_declaration.decl_type

    vtype, content = get_stored_value_and_type_for(tip_decl, access_string=generate_assignment_path_for(node.target) if not isinstance(tip, ObjectLocatorDelegated) else "{}", duplicate=not isinstance(tip, ObjectLocatorDelegated))

    output.add({
        ValueTypePrimitive.RAW_STRING: "if (v.type != json::Value::STR) ",
        ValueTypePrimitive.FLOATING_POINT: "if (!v.is_number()) ",
        ValueTypePrimitive.INTEGER: "if (v.type != json::Value::INT) ",
        ValueTypePrimitive.BOOLEAN: "if (v.type != json::Value::BOOL) ",
    }[vtype] + "goto unk_type;")

    # generate actions
    for action in node.entry_actions:
        generate_entry_action(output, action, node)

    output.add(generate_assignment_statement_for(node.target, content))

def generate_json_callback(output, node_to_symbol_map, enumname="cfgstate", quiet=True):
    """
    structure is one large switch statement, like this:

    switch (curnode) {
        case cfgstate::SYMBOL:
            // 
    }
    """
    
    output.add("switch (curnode) {")
    # Visit all 
    for node, symbol in node_to_symbol_map.items():
        # Generate start actions
        output.add(f"jpto_{symbol}:")
        output.add(f"case {enumname}::{symbol}:")
        with output as body:
            for action in node.entry_actions:
                generate_entry_action(body, action, node)
            # Add node set line
            body.add(f"curnode = {enumname}::{symbol};")
            body.add()
            has_if = False
            # Generate all subnode entries (for jumps)
            for leaf in node.leaves:
                conditions = [f"stack_ptr {'>=' if isinstance(leaf, DelegateJsonPathNode) else '=='} {node.node_depth + 2}"]
                if not leaf.is_nonconst_name:
                    conditions.append(f"!strcmp(stack[{leaf.node_depth}]->name, {as_c_string(leaf.name)})")
                if isinstance(leaf.name, BaseJsonPathNode.DynName):
                    conditions.append(f"!strcmp(stack[{leaf.node_depth}]->name, {leaf.name.var})")
                if leaf.is_array:
                    conditions.append(f"stack[{leaf.node_depth}]->is_array()")
                if leaf.array_index >= 0:
                    conditions.append(f"stack[{leaf.node_depth}]->index == {leaf.array_index}")
                if leaf.array_maximum is not None:
                    conditions.append(f"stack[{leaf.node_depth}]->index < {leaf.array_maximum}")
                if leaf.array_index == BaseJsonPathNode.DYNAMIC_ARRAY:
                    conditions.append(f"stack[{leaf.node_depth}]->index == {leaf.array_dynamic_target}")
                body.add("//", leaf)
                body.add(f"{'if' if not has_if else 'else if'} ({' && '.join(conditions)}) {{")
                has_if = True
                with body as leafbody:
                    # If this is a delegate, just jump straight to it
                    if isinstance(leaf, DelegateJsonPathNode):
                        leafbody.add(f"goto jpto_{node_to_symbol_map[leaf]};")
                    else:
                        # Otherwise, we have to generate the code for the store.
                        generate_store_code(leafbody, leaf)
                body.add("}")
            # Generate return to parent node -- whenever we see a value with our position on it, we go up a level.
            # TODO: should this also explicitly check for value_type == OBJ?
            body.add(f"{'if' if not has_if else 'else if'} (stack_ptr == {node.node_depth+1}) {{")
            with body as endbody:
                if node.parent_node in node_to_symbol_map:
                    endbody.add(f"curnode = {enumname}::{node_to_symbol_map[node.parent_node]};")
                endbody.add("return;")
            body.add("}")
            # If we get out of this if, we warn in logs
            body.add('else { goto unk_tag; }')
            body.add("return;")
            body.add()
                
    output.add("}")
    output.add("return;")
    output.add("unk_tag:");
    if not quiet: output.add('ESP_LOGD(TAG, "unknown tag in config %s", stack[stack_ptr-1]->name);')
    output.add("return;")
    output.add("unk_type:");
    output.add('if (v.type != json::Value::NONE) ESP_LOGW(TAG, "wrong type for %s", stack[stack_ptr-1]->name);')
    output.add("return;")

def generate_parser(output, nodemap):
    output.add("// ACTUAL PARSER")
    output.add("bool parse_config(json::TextCallback&& tcb) {")
    with output as inner:
        # Declare state var
        inner.add("cfgstate curnode = cfgstate::R;")
        inner.add()
        inner.add("clear_config();")
        inner.add("json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){");
        with inner as fparser:
            generate_json_callback(fparser, nodemap)
        inner.add("});")
        inner.add()
        inner.add("if (!parser.parse(std::move(tcb))) {")
        with inner as bad:
            bad.add('ESP_LOGE(TAG, "Failed to parse JSON for config");')
            bad.add("return false;")
        inner.add("}")
        inner.add("return true;")
    output.add("}")

def generate_result_sender(output, decl, tgt):
    if pygccxml.declarations.is_bool(decl.return_type):
        output.add("return ", tgt, ";")

def generate_subparser(output, decl, nodemap, structname):
    output.add(f"// SUBPARSER FOR {decl}")
    # in theory this should work? afaict it generates a perfectly valid string
    nm = pygccxml.declarations.declaration_utils.full_name(decl)
    if nm.startswith("::"):
        nm = nm[2:]
    output.add(
        decl.return_type,
        nm,
        "(" + ", ".join(str(x if j != collected_loader_properties[decl][0] else x.clone(name="returnobj")) for j, x in enumerate(decl.arguments)) + ")", "{"
    )
    with output as inner:
        inner.add(f"{structname} curnode = {structname}::R;")
        inner.add()
        inner.add("json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){");
        with inner as fparser:
            generate_json_callback(fparser, nodemap, structname, True)
        inner.add("});")
        inner.add();
        inner.add("if (!parser.parse(config::sd_cfg_load_source())) {")
        with inner as bad:
            bad.add('ESP_LOGE(TAG, "Failed to parse JSON for config");')
            generate_result_sender(bad, decl, "false")
        inner.add("}")
        inner.add("else {")
        with inner as good:
            generate_result_sender(good, decl, "true")
        inner.add("}")
    output.add("}")

def generate_code(output):
    generate_header(output)
    output.add()
    generate_variable_declarations(output)
    output.add()
    output.add("namespace config {")
    parseroutput = Outputter()
    nodenodemapmap = {}
    nodenodenamemap = {}
    with output as nsoutput:
        root_nodemap = generate_nodeenum_declaration(nsoutput, options_tree)
        nsoutput.add()
        for decl in collected_loader_nodes:
            nodenodenamemap[decl] = generate_subparser_state_name(collected_loader_properties[decl][1])
            nodenodemapmap[decl] = generate_nodeenum_declaration(nsoutput, collected_loader_nodes[decl], nodenodenamemap[decl], "SUBPARSER STATE ENUM")
            nsoutput.add()
        generate_redefault_function(nsoutput)
        nsoutput.add()
        generate_parser(parseroutput, root_nodemap)
        generate_str_to_enum_funcs(nsoutput)
        nsoutput.add()
        nsoutput += parseroutput.value()
    output.add("}")
    output.add()
    for decl in collected_loader_nodes:
        generate_subparser(output, decl, nodenodemapmap[decl], "config::" + nodenodenamemap[decl])
        output.add()

with open(sys.argv[-1], 'w') as f:
    generate_code(Outputter(target=f))
