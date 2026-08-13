# Night Course translucent unlit fade material (Color / Opacity / FadeAlpha)
import unreal

ASSET_PATH = "/Game/Night/Course/Materials"
ASSET_NAME = "M_NightUnlitFade"
FULL = f"{ASSET_PATH}/{ASSET_NAME}"

def main():
    if unreal.EditorAssetLibrary.does_asset_exist(FULL):
        unreal.EditorAssetLibrary.delete_asset(FULL)

    factory = unreal.MaterialFactoryNew()
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME, ASSET_PATH, unreal.Material, factory
    )
    if not mat:
        unreal.log_error("Failed to create M_NightUnlitFade")
        return

    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    # Mobile-friendly translucent sorting
    try:
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    except Exception:
        pass

    color = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -480, -40
    )
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.55, 0.55, 0.62, 1.0))

    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -480, 160
    )
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 1.0)

    fade = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -480, 300
    )
    fade.set_editor_property("parameter_name", "FadeAlpha")
    fade.set_editor_property("default_value", 1.0)

    mul = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -220, 220
    )

    unreal.MaterialEditingLibrary.connect_material_expressions(opacity, "", mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fade, "", mul, "B")
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.layout_material_expressions(mat)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(FULL)
    unreal.log(f"Created {FULL}")

if __name__ == "__main__":
    main()
