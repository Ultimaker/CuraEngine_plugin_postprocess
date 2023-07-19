# Copyright (c) 2023 UltiMaker
# Cura is released under the terms of the LGPLv3 or higher.

import os
import stat
import platform
from pathlib import Path

from UM.i18n import i18nCatalog
from UM.Logger import Logger
from cura.BackendPlugin import BackendPlugin

catalog = i18nCatalog("cura")

class CuraEnginePostprocess(BackendPlugin):
    def __init__(self):
        super().__init__()
        if not self.binaryPath().exists():
            Logger.error(f"Could not find CuraEnginePostprocess binary at {self.binaryPath().as_posix()}")
        if platform.system() != "Windows":
            st = os.stat(self.binaryPath())
            os.chmod(self.binaryPath(), st.st_mode | stat.S_IEXEC)

        self._plugin_command = [self.binaryPath().as_posix()]
        self._supported_slots = [101]  # ModifyPostprocess SlotID

    def binaryPath(self) -> Path:
        ext = ".exe" if platform.system() == "Windows" else ""
        return Path(__file__).parent.joinpath({ "AMD64": "x86_64", "x86_64": "x86_64" }.get(platform.machine()), platform.system(), f"curaengine_plugin_postprocess{ext}").resolve()
