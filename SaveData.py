from ctypes import *
from enum import Enum
import sys

class SaveSection(Enum):
    UNDEFINED = 0x00
    # Container type UNDEFINED if none of them
    STRING_MAP = 0x01 # string map (hard limit : 65535 entries)
    ADDRESS_MAP = 0x02 # uint64_t map (hard limit : 65535 entries)
    LIST = 0x03 # list (hard limit : 65535 entries)
    POINTER = 0x40 # pointer to another SaveData (not implemented yet)
    SHORT_MAP = 0x42 # Map of uint16_t (hard limit : 65535 entries)
    WIDE_LIST = 0x43 # List (hard limit : 4294967295 entries)
    # Attached data size length UNDEFINED for no attached data
    CHAR_SIZE = 0x04
    SHORT_SIZE = 0x08
    INT_SIZE = 0x0c
    # Attached data type for special data only
    SUBFILE = 0x10
    # This object hold a reference in the BigSave reference table
    REFERENCED = 0x20
    # Use an extended type
    EXTENDED_TYPE = 0x80

TYPE_MASK = 0x43
SIZE_MASK = 0x0c
SPECIAL_MASK = 0x10

vec3 = c_double * 3
c_str = c_void_p

class SaveData:
    """SaveData interface with C++, note that methods start with _
    The operator[](const std::string &key) is the . operator
    The operator[](uint64_t address) is the [] operator
    """
    _lib = None
    _ownref = False

    def init(lib):
        "Initialize the SaveData system"
        SaveData._lib = lib
        SaveData._lib.sd_strmap.restype = c_void_p
        SaveData._lib.sd_nbrmap.restype = c_void_p
        SaveData._lib.sd_new.restype = c_void_p
        SaveData._lib.sd_get.restype = c_void_p
        SaveData._lib.sd_file.restype = c_void_p
        SaveData._lib.sd_getraw.restype = c_void_p
        SaveData._lib.sd_computesize.restype = c_uint64
        SaveData._lib.sd_repr.restype = c_char_p
        SaveData._lib.sd_size.restype = c_int
        SaveData._lib.sd_push.restype = c_int
        SaveData._lib.sd_copy.restype = None
        SaveData._lib.sd_delete.restype = None
        SaveData._lib.sd_set.restype = None
        SaveData._lib.sd_save.restype = None
        SaveData._lib.sd_load.restype = None
        SaveData._lib.sd_truncate.restype = None
        SaveData._lib.sd_reset.restype = None
        SaveData._lib.sd_close.restype = None

        SaveData._lib.bs_new.restype = c_void_p
        SaveData._lib.bs_delete.restype = None
        SaveData._lib.bs_open.restype = c_bool
        SaveData._lib.bs_store.restype = c_bool

    def detach():
        "Detach the SaveData library"
        SaveData._lib = None

    def __init__(self, ptr = None, ownRef = True):
        self._ = SaveData._lib
        if ptr is None:
            self._ref = self._.sd_new()
        else:
            self._ref = ptr
        self._ownref = ownRef

    def __del__(self):
        if self._ownref:
            self._.sd_delete(c_void_p(self._ref))

    def __repr__(self):
        return str(self._.sd_repr(c_void_p(self._ref)), "utf-8");

    # for [] operator, usefull for integral entries
    def __getitem__(self, key):
        if type(key) is str:
            return SaveData(self._.sd_strmap(c_void_p(self._ref), c_char_p(bytes(name, encoding="utf-8"))), False)
        else:
            return SaveData(self._.sd_nbrmap(c_void_p(self._ref), c_uint64(key)), False)

    def __setitem__(self, key, value):
        if type(key) is str:
            if type(value) is SaveData:
                self._.sd_copy(self._.sd_strmap(c_void_p(self._ref), c_char_p(bytes(name, encoding="utf-8"))), c_void_p(value._ref))
            else:
                self._.sd_set(self._.sd_strmap(c_void_p(self._ref), c_char_p(bytes(name, encoding="utf-8"))), byref(value), sizeof(value))
        else:
            if type(value) is SaveData:
                self._.sd_copy(self._.sd_nbrmap(c_void_p(self._ref), c_uint64(key)), c_void_p(value._ref))
            else:
                self._.sd_set(self._.sd_nbrmap(c_void_p(self._ref), c_uint64(key)), byref(value), sizeof(value))

    def append(self, value):
        if type(value) is SaveData:
            return self._.sd_push(c_void_p(self._ref), c_void_p(value._ref))
        else:
            raise TypeError("Value must be a SaveData")

    def get(self, ctype):
        "Return the stored data of the given type"
        return cast(self._.sd_get(c_void_p(self._ref), sizeof(ctype)), POINTER(ctype)).contents

    def raw(self):
        "Return the stored bytes as a c_char array"
        sz = c_int()
        return cast(self._.sd_getraw(c_void_p(self._ref), byref(sz)), c_char * sz)

    def set(self, value):
        "Store the given bytes or c_type in this SaveData"
        if type(value) is bytes:
            self._.sd_set(c_void_p(self._ref), cast(value, c_void_p), len(value))
        else:
            self._.sd_set(c_void_p(self._ref), byref(value), sizeof(value))

    def __len__(self):
        "Return the number of SaveData directly attached to this SaveData"
        self._.sd_size(c_void_p(self._ref))

    def load(self, buffer):
        "Load a SaveData from a buffer"
        self._.sd_load(c_void_p(self._ref), cast(buffer, c_void_p))

    def save(self):
        "Save a SaveData to a c_char array and return it"
        ret = c_char * self._.sd_computesize(c_void_p(self._ref))
        self._.sd_save(c_void_p(self._ref), ret)
        return ret

    def copy(self):
        "Copy this SaveData"
        ret = SaveData();
        self._.sd_copy(ret._ref, self._ref)
        return ret

    def file(self, path=None):
        if (path is None):
            return BigSave(self._.sd_file(ret._ref, c_void_p(0)), False);
        else:
            return BigSave(self._.sd_file(ret._ref, c_char_p(bytes(path, encoding="utf-8"))), False);
