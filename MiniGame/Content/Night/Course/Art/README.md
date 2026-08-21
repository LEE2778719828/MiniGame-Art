# Art import targets (G3.5)

Import FBX from the categorized `ArtSubmit/` folders into these folders, then bind
the resulting Blueprint prefabs manually on the Atom BP components. Do not put
mesh paths in planner rules or temporary Proc assets.

Expected asset names (examples):

- `/Game/Night/Course/Art/Bridge/muban1`
- `/Game/Night/Course/Art/Bridge/muban2`
- `/Game/Night/Course/Art/Hero/zhujue`
- `/Game/Night/Course/Art/Foe/fish_moneter`
- `/Game/Night/Course/Art/Foe/cantingguai`
- `/Game/Night/Course/Art/Environment/canguan`

Until a visual prefab is bound, runtime intentionally keeps an empty/native
gameplay carrier; it does not fall back to an old mesh configuration.
