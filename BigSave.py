from SaveData import *

class BigSave(SaveData):
    def __init__(self, ptr = None, parent = None):
        self._ = SaveData._lib
        if ptr is None:
            self._ref = self._.bs_new()
        else:
            self._ref = ptr
        self._parent = parent

    def __del__(self):
        if self._parent is None:
            self._.bs_delete(c_void_p(self._ref))

    def open(self, saveName, _saveAtDestroy=True, _reduceWrite=True, _reducedCheck=False):
        return self._.bs_open(c_void_p(self._ref), c_char_p(bytes(saveName, "utf8")), c_bool(_saveAtDestroy), c_bool(_reduceWrite), c_bool(_reducedCheck))

    def store(self):
        return self._.bs_store(c_void_p(self.ref))
