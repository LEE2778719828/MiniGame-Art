"""Probe how this editor lets Python rewrite a DataTable, and how it prints a Transform."""

import unreal


table = unreal.load_asset("/Game/Day/Data/DT_SDayBoardLayout")
unreal.log("[Probe] table = {}".format(table))
unreal.log("[Probe] DataTableFunctionLibrary = {}".format(
    [n for n in dir(unreal.DataTableFunctionLibrary) if not n.startswith("_")]))
unreal.log("[Probe] row names = {}".format(
    unreal.DataTableFunctionLibrary.get_data_table_row_names(table)))
unreal.log("[Probe] Transform column = {}".format(
    unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "Transform")))
