# Copyright (c) 2023 UltiMaker
# Cura is released under the terms of the LGPLv3 or higher.

from . import CuraEnginePostprocess

from UM.i18n import i18nCatalog
catalog = i18nCatalog("cura")


def getMetaData():
    return {}

def register(app):
    return { "backend_plugin": CuraEnginePostprocess.CuraEnginePostprocess() }