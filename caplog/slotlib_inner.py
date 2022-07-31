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

    def __repr__(self):
        return f"DeclaredMemberStruct({self.name!r}, {self.members!r})"

class DeclaredMember:
    def __init__(self, name, typetoken, bitfield=None, enumer=None):
        self.name = name
        self.typetoken = typetoken
        if bitfield and len(self.typetoken) > 1:
            raise NotImplementedError("bitfield defined in array not supported")
        if bitfield and enumer:
            raise NotImplementedError("cannot have bitfield with enumeration")
        if enumer and len(self.typetoken) > 1:
            enumer = None
        self.bitfield = bitfield
        self.enumer = {y: x for x, y in enumer.items()} if enumer else None

        self.data = None
        self.rawdata = None

    def __repr__(self):
        enumer = {y: x for x, y in self.enumer.items()} if self.enumer else None
        return f"DeclaredMember({self.name!r}, {self.typetoken!r}, {self.bitfield!r}, {enumer!r})"

    def is_array(self):
        return len(self.typetoken) > 1

    def is_bitfield(self):
        return self.bitfield is not None

    def is_enumeration(self):
        return self.enumer is not None

    def is_floating_point(self):
        return self.typetoken[0] in ["f", "d"]

    def parse(self, segment_of_dat):
        if not self.is_array() and not self.is_bitfield() and not self.is_enumeration():
            self.data = struct.unpack("<" + self.typetoken, segment_of_dat)[0]
        elif self.is_bitfield():
            self.data = {}
            integral_value = struct.unpack("<" + self.typetoken, segment_of_dat)[0]
            for i in self.bitfield:
                if integral_value & self.bitfield[i]:
                    self.data[i] = True
                else:
                    self.data[i] = False
            self.rawdata = integral_value
        elif self.is_enumeration():
            self.rawdata = struct.unpack("<" + self.typetoken, segment_of_dat)[0]
            self.data = self.enumer.get(self.rawdata, "<UNK: {val:0{hexwidth}x} ({val})>".format(val=self.rawdata, hexwidth=struct.calcsize(self.typetoken)*2))
        else:
            self.data = struct.unpack("<" + self.typetoken, segment_of_dat)

    def raw_value(self):
        if self.is_bitfield() or self.is_enumeration():
            return self.rawdata
        else:
            return self.data

    def formatted_value(self):
        """
        Returns the object in a formatted form, including name
        """

        if not self.is_array() and not self.is_bitfield() and not self.is_enumeration():
            if self.typetoken == '?':
                return "{name}: {bval} ({val})".format(name=self.name, val=int(self.data), bval=self.data)
            elif not self.is_floating_point():
                width = struct.calcsize(self.typetoken) * 2

                return "{name}: {val:0{hexwidth}x} ({val})".format(name=self.name, val=self.data, hexwidth=width)
            else:
                return "{name}: {val}{typetoken}".format(name=self.name, val=self.data, typetoken=self.typetoken[0])
        elif self.is_bitfield():
            return "{name}: {val:b} > {params}".format(name=self.name, val=self.rawdata, params=",".join(x for x in self.data if self.data[x]))
        elif self.is_enumeration():
            return "{name}: {val}".format(name=self.name, val=self.data)
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

    def __repr__(self):
        return f"SlotType({self.members!r})"
        
    def parse(self, dat):
        member_copy = copy.deepcopy(self.members)
        for member, idx in zip(member_copy, self.member_decode_slices):
            member.parse(dat[idx])
        return member_copy

    def get_formatted(self, data, min_pos=0, max_pos=0):
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

    def __repr__(self):
        if self.length != SlotTypeArray.VARLEN:
            return f"SlotTypeArray({self.subtype!r})"
        else:
            return f"SlotTypeArray({self.subtype!r}, {self.length!r})"

    def parse(self, dat):
        values = []
        remain = len(dat)
        pos = 0
        while remain and (self.length == SlotTypeArray.VARLEN or len(values) < self.length):
            amt = self.subtype.get_length(dat[pos:])
            remain -= amt
            if remain < 0:
                raise ValueError("invalid size for varlen")
            values.append((self.subtype.parse(dat[pos:pos+amt]), pos))
            pos += amt
        return values

    def get_formatted(self, data, min_pos=0, max_pos=0):
        response = ""
        for j, (val, position) in enumerate(data):
            if min_pos and max_pos and not (min_pos <= position <= max_pos):
                continue
            init_indent = f"[{j}]: "
            text = textwrap.indent(self.subtype.get_formatted(val), " "*len(init_indent))
            text = init_indent + text[len(init_indent):]
            response += text + "\n"
        return response[:-1]

class SlotTypeString:
    def __repr__(self):
        return "SlotTypeString()"

    def parse(self, dat):
        if b'\x00' in dat:
            length = dat.index(b'\x00')
            return dat[:length]
        else:
            return dat

    def get_formatted(self, data, min_pos=0, max_pos=0):
        return repr(data.decode("ascii"))

    def get_length(self, dat):
        if b'\x00' in dat:
            if dat[0] == 0:  # coalesce consecutive cavities
                leng = 0
                while dat[leng] == 0: leng += 1
                return leng
            else:
                return dat.index(b'\x00') + 1
        else:
            return len(dat)

