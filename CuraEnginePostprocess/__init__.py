# Copyright (c) 2023 UltiMaker
# Cura is released under the terms of the LGPLv3 or higher.

import platform

if platform.machine() in ["AMD64", "x86_64"]:
    from . import CuraEnginePostprocess

    from UM.i18n import i18nCatalog
    catalog = i18nCatalog("cura")

    def getMetaData():
        return {}

    def register(app):
        return { "backend_plugin": CuraEnginePostprocess.CuraEnginePostprocess() }
else:
    from UM.Logger import Logger
    Logger.error("CuraEnginePostprocess plugin is only supported on x86_64 systems")

    def getMetaData():
        return {}

    def register(app):
        return {}