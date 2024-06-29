from pygccxml import utils
from pygccxml import declarations
from pygccxml import parser
import re
import time
import os

from slotlib_inner import SlotType, SlotTypeArray, SlotTypeString, DeclaredMember, DeclaredMemberStruct, SlotTypeBits

def snake_to_camel(text):
    return "".join(x.title() for x in text.split("_"))

with open(os.path.join(os.path.dirname(__file__), "../stm/src/common/slots.h"), "r") as f:
    full_text = f.read()

# parse the file
gp, gn = utils.find_xml_generator()
xml_generator_config = parser.xml_generator_configuration_t(
    cflags="--std=c++17",
    xml_generator_path=gp,
    xml_generator=gn)

declarations_in_file = parser.parse_string(full_text, xml_generator_config)

def _create_member_list(struct_info):
    new_type_members = []
    for i in struct_info.variables():
        if i.type_qualifiers.has_static:
            continue
        bitfield = None
        enumer = None
        if declarations.is_integral(i.decl_type):
            try:
                for enumerator in struct_info.enumerations():
                    if enumerator.byte_size != declarations.remove_alias(i.decl_type).byte_size:
                        continue
                    elif enumerator.name != snake_to_camel(i.name):
                        continue
                    else:
                        bitfield = enumerator.get_name2value_dict()
                        break;
            except RuntimeError:
                bitfield = None
        elif declarations.is_enum(i.decl_type):
            try:
                enumer = global_namespace.enumeration(i.decl_type.decl_string).get_name2value_dict()
            except RuntimeError:
                enumer = {}

        if declarations.is_class(i.decl_type):
            new_type_members.append(DeclaredMemberStruct(i.name, _create_member_list(i.decl_type.declaration)))
        else:
            new_type_members.append(DeclaredMember(i.name, _convert_opaque_to_token(i.decl_type), bitfield=bitfield, enumer=enumer, special_hook=_lookup_special_printer(i.decl_type)))
    return new_type_members

def _convert_opaque_to_token(x):
    """
    convert either a string from re or a value from pygccxml to a struct token
    """

    if type(x) is str:
        return {
            "BOOL": "?",
            "UINT8_T": "B",
            "INT8_T": "b",
            "UINT16_T": "H",
            "INT16_T": "h",
            "UINT32_T": "I",
            "INT32_T": "i",
            "UINT64_T": "Q",
            "INT64_T": "q",
            "FLOAT": "f",
            "DOUBLE": "d"
        }[x.upper()]
    elif isinstance(x, declarations.declarated_t) and not declarations.is_enum(x):
        return _convert_opaque_to_token(declarations.remove_alias(x))
    elif declarations.is_integral(x) or declarations.is_enum(x):
        basetoken = {
            1: "b",
            2: "h",
            4: "i",
            8: "q"
        }[int(x.byte_size)]

        try:
            if "unsigned" in x.CPPNAME:
                basetoken = basetoken.upper()
            
            if "bool" in x.CPPNAME:
                basetoken = '?'
        except AttributeError:
            pass
        
        return basetoken
    elif declarations.is_floating_point(x):
        if isinstance(x, declarations.float_t):
            return "f"
        else:
            return "d"
    elif declarations.is_array(x):
        basetoken = _convert_opaque_to_token(declarations.array_item_type(x))
        return basetoken * x.size
    else:
        raise ValueError("unknown instance: " + repr(x))

# don't you just love regexes?
comment_interpreter = re.compile(r"^\s*(\w+)\s*=\s*0x\w+,\s*//\s+([A-Z0-9_']+(?:\[\])?)(?:;[^\S\n\r]*(\w+)?)?(.+)?$", re.MULTILINE)

"""
primary dict of type info
"""
slot_types = {}

comment_matches = comment_interpreter.findall(full_text)
comment_matches_indexed = {}
for i in comment_matches:
    comment_matches_indexed[i[0]] = i[1:]
global_namespace = declarations.get_global_namespace(declarations_in_file)
slots_namespace = global_namespace.namespace("slots")
dataid_enum = slots_namespace.enumeration("DataID")
previous_match = None
special_printers = {
    slots_namespace.typedef("millis_timestamp"): lambda t: f"{t} ({time.asctime(time.gmtime(t // 1000))} +{t % 1000}ms)",
    slots_namespace.typedef("byte_percent"): lambda t: f"{t} ({t / 2.55:.1f}%)"
}

def _lookup_special_printer(x):
    if not isinstance(x, declarations.declarated_t):
        return None

    return special_printers.get(x.declaration)

def _handle_struct(entryname, struct_name):
    struct_info = slots_namespace.class_(struct_name)

    new_type_members = _create_member_list(struct_info)

    slot_types[entryname][1] = SlotType(new_type_members)

def _handle_enum(entryname, enum_name):
    enum_info = slots_namespace.enumeration(enum_name)

    enumer = enum_info.get_name2value_dict()

    slot_types[entryname][1] = SlotType(
        [DeclaredMember("", _convert_opaque_to_token(enum_info), enumer=enumer)]
    )

def _handle_match(entryname, match_data):
    global slot_types
    global previous_match

    if match_data[0].endswith("[]"):
        _handle_match(entryname, (match_data[0].rstrip("[]"), *match_data[1:]))
        slot_types[entryname][1] = SlotTypeArray(slot_types[entryname][1])
    elif len(match_data) == 1:
        match_type = match_data[0]
        if match_type in ["STRUCT", "ENUM"]:
            raise SyntaxError("invalid: struct or enum without typename")
        elif match_type in ["''", '"']:
            _handle_match(entryname, previous_match)
        elif match_type == "STRING":
            slot_types[entryname][1] = SlotTypeString()
        elif match_type == "BITS":
            slot_types[entryname][1] = SlotTypeBits()
        else:
            # create dummy with one member
            slot_types[entryname][1] = \
                SlotType(
                        [DeclaredMember("", _convert_opaque_to_token(match_type))]
                )
    elif len(match_data) == 2:
        if match_data[0] == "ENUM":
            _handle_enum(entryname, match_data[1])
        elif match_data[0] != "STRUCT":
            return _handle_match(entryname, [match_data[0]])
        else:
            _handle_struct(entryname, match_data[1])
    else:
        _handle_match(entryname, match_data[:2])

    if match_data[0] not in ('"', "''"):
        previous_match = match_data

for entry in dataid_enum.values:
    slot_types[entry[1]] = [entry[0], None]
    if entry[0] not in comment_matches_indexed:
        raise RuntimeError("couldn't find index " + str(entry[0]))

    _handle_match(entry[1], comment_matches_indexed[entry[0]])
