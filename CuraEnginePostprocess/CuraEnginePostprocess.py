# Copyright (c) 2023 UltiMaker
# Cura is released under the terms of the LGPLv3 or higher.


import platform
from pathlib import Path
from UM.i18n import i18nCatalog

from cura.BackendPlugin import BackendPlugin

catalog = i18nCatalog("cura")


class CuraEnginePostprocess(BackendPlugin):
    def __init__(self):
        super().__init__()
        self._plugin_command = [self.binaryPath()]
        self._supported_slots = [101]  # ModifyPostprocess SlotID

    def binaryPath(self) -> str:
        ext = ".exe" if platform.system() == "Windows" else ""
        return Path(__file__).parent.joinpath(platform.machine(), platform.system(), f"curaengine_plugin_postprocess{ext}").resolve().as_posix()
