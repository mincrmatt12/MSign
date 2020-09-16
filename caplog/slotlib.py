from pygccxml import utils
from pygccxml import declarations
from pygccxml import parser
import re
import os
import struct
import binascii
import copy
import textwrap

class DeclaredMemberStruct:
    def __init__(self, name, members):
        self.name = name
        self.members = members
        self.decodestr = "<" + "".join(x.typetoken for x in members)
        self.typetoken = f"{struct.calcsize(self.decodestr)}s"
        self.member_decode_slices = []
        idx = 0
        for member in members:
            self.member_decode_slices.append(slice(idx, idx + struct.calcsize(member.typetoken)))
            idx += struct.calcsize(member.typetoken)

    def parse(self, segment):
        parsed_args = struct.unpack(self.decodestr, segment)
        for member, idx in zip(self.members, self.member_decode_slices):
            member.parse(segment[idx])

    def formatted_value(self):
        init_indent = f"{self.name}: "
        text = "\n".join(x.formatted_value() for x in self.members)
        text = textwrap.indent(text, " "*len(init_indent))
        text = init_indent + text[len(init_indent):]
        return text

class DeclaredMember:
    def __init__(self, name, typetoken, bitfield=None):
        self.name = name
        self.typetoken = typetoken
        if bitfield and len(self.typetoken) > 1:
            raise NotImplementedError("bitfield defined in array not supported")
        self.bitfield = bitfield

        self.data = None
        self.rawdata = None

    def is_array(self):
        return len(self.typetoken) > 1

    def is_bitfield(self):
        return self.bitfield is not None

    def is_floating_point(self):
        return self.typetoken[0] in ["f", "d"]

    def parse(self, segment_of_struct_args):
        if not self.is_array() and not self.is_bitfield():
            self.data = segment_of_struct_args[0]
        elif self.is_bitfield():
            self.data = {}
            integral_value = segment_of_struct_args[0]
            for i in self.bitfield:
                if integral_value & self.bitfield[i]:
                    self.data[i] = True
                else:
                    self.data[i] = False
            self.rawdata = integral_value
        else:
            self.data = segment_of_struct_args

    def raw_value(self):
        if self.is_bitfield():
            return self.rawdata
        else:
            return self.data

    def formatted_value(self):
        """
        Returns the object in a formatted form, including name
        """

        if not self.is_array() and not self.is_bitfield():
            if self.typetoken == '?':
                return "{name}: {bval} ({val})".format(name=self.name, val=int(self.data), bval=self.data)
            elif not self.is_floating_point():
                width = struct.calcsize(self.typetoken) * 2

                return "{name}: {val:0{hexwidth}x} ({val})".format(name=self.name, val=self.data, hexwidth=width)
            else:
                return "{name}: {val}{typetoken}".format(name=self.name, val=self.data, typetoken=self.typetoken[0])
        elif self.is_bitfield():
            return "{name}: {val:b} > {params}".format(name=self.name, val=self.rawdata, params=",".join(x for x in self.data if self.data[x]))
        else:
            if self.typetoken[0] in ["B", "c"] and ("len" not in self.name.lower() and "size" not in self.name.lower()):
                if self.typetoken[0] == "B":
                    combined = bytes(self.data)
                else:
                    combined = b''.join(self.data)

                asc = repr(combined)[1:]
                val = binascii.hexlify(combined).decode('ascii')

                return "{name}: {asc} ({val})".format(name=self.name, asc=asc, val=val)
            else:
                if not self.is_floating_point():
                    width = struct.calcsize(self.typetoken[0]) * 2
                    return "{}: {}".format(self.name, ", ".join("{val:0{width}x} ({val})".format(width=width, val=x) for x in self.data))
                else:
                    return "{}: {}".format(self.name, ", ".join(str(x) for x in self.data))


class SlotType:
    def __init__(self, members):
        self.members = members
        self.decodestr = "".join(x.typetoken for x in members)
        idx = 0
        self.member_decode_slices = []
        for member in members:
            self.member_decode_slices.append(slice(idx, idx + struct.calcsize(member.typetoken)))
            idx += struct.calcsize(member.typetoken)
        
    def parse(self, dat):
        member_copy = copy.deepcopy(self.members)
        for member, idx in zip(member_copy, self.member_decode_slices):
            member.parse(dat[idx])
        return member_copy

    def get_formatted(self, data):
        if len(data) > 1:
            return "\n".join(x.formatted_value() for x in data)
        else:
            return data[0].formatted_value()[(len(data[0].name) + 2):]

    def get_length(self, dat):
        return struct.calcsize("<" + self.decodestr)

class SlotTypeArray:
    VARLEN = 0

    def __init__(self, subtype, length=VARLEN):
        self.subtype = subtype
        self.length = length

    def parse(self, dat):
        values = []
        remain = len(dat)
        pos = 0
        while remain and (self.length == SlotTypeArray.VARLEN or len(values) < self.length):
            amt = self.subtype.get_length(dat[pos:])
            remain -= amt
            if remain < 0:
                raise ValueError("invalid size for varlen")
            values.append(self.subtype.parse(dat[pos:pos+amt]))
            pos += amt
        return values

    def get_formatted(self, data):
        response = ""
        for j, val in enumerate(data):
            init_indent = f"[{j}]: "
            text = textwrap.indent(self.subtype.get_formatted(val), " "*len(init_indent))
            text = init_indent + text[len(init_indent):]
            response += text + "\n"
        return response[:-1]

class SlotTypeString:
    def parse(self, dat):
        if b'\x00' in dat:
            length = dat.index(b'\x00')
            return dat[:length]
        else:
            return dat

    def get_formatted(self, data):
        return repr(data)[1:]

    def get_length(self, dat):
        if b'\x00' in dat:
            return dat.index(b'\x00') + 1
        else:
            return len(dat)



with open(os.path.join(os.path.dirname(__file__), "../stm/src/common/slots.h"), "r") as f:
    full_text = f.read()


# parse the file
gp, gn = utils.find_xml_generator()
xml_generator_config = parser.xml_generator_configuration_t(
    xml_generator_path=gp,
    xml_generator=gn)

declarations_in_file = parser.parse_string(full_text, xml_generator_config)

def _create_member_list(struct_info):
    new_type_members = []
    for i in struct_info.variables():
        bitfield = None
        if declarations.is_integral(i.decl_type):
            try:
                enumerator = struct_info.enumerations()[0]
                bitfield = enumerator.get_name2value_dict()
            except RuntimeError:
                bitfield = None

        if declarations.is_class(i.decl_type):
            new_type_members.append(DeclaredMemberStruct(i.name, _create_member_list(i.decl_type.declaration)))
        else:
            new_type_members.append(DeclaredMember(i.name, _convert_opaque_to_token(i.decl_type), bitfield))
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
    elif isinstance(x, declarations.declarated_t):
        return _convert_opaque_to_token(declarations.remove_alias(x))
    elif declarations.is_integral(x):
        basetoken = {
            1: "b",
            2: "h",
            4: "i",
            8: "q"
        }[int(x.byte_size)]

        if "unsigned" in x.CPPNAME:
            basetoken = basetoken.upper()
        
        if "bool" in x.CPPNAME:
            basetoken = '?'
        
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

def _handle_struct(entryname, struct_name):
    struct_info = slots_namespace.class_(struct_name)

    new_type_members = _create_member_list(struct_info)

    slot_types[entryname][1] = SlotType(new_type_members)

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
        else:
            # create dummy with one member
            slot_types[entryname][1] = \
                SlotType(
                        [DeclaredMember("", _convert_opaque_to_token(match_type))]
                )
    elif len(match_data) == 2:
        if match_data[0] == "ENUM":
            raise NotImplementedError("don't do enums")
        elif match_data[0] != "STRUCT":
            return _handle_match(entryname, [match_data[0]])
        else:
            _handle_struct(entryname, match_data[1])
    else:
        print(entryname, match_data)
        _handle_match(entryname, match_data[:2])

    if match_data[0] not in ('"', "''"):
        previous_match = match_data

for entry in dataid_enum.values:
    slot_types[entry[1]] = [entry[0], None]
    if entry[0] not in comment_matches_indexed:
        raise RuntimeError("couldn't find index " + str(entry[0]))

    _handle_match(entry[1], comment_matches_indexed[entry[0]])
