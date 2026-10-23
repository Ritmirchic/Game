# Ассеты и атрибуция

Все перечисленные файлы хранятся локально в `public/assets/` и встраиваются в автономные `mirage.html` и ранее сохранённый `last-beacon.html`. Во время игры нет загрузок с внешних CDN. «МИРАЖ» повторно использует Soldier, Fox, сухое дерево, environment-карту и текстуры; остальные ассеты также сохранены в репозитории.

## BabylonJS / Assets

Файлы получены из репозитория [BabylonJS/Assets](https://github.com/BabylonJS/Assets). Корневые [README](https://github.com/BabylonJS/Assets/blob/master/README.md) и [LICENSE](https://github.com/BabylonJS/Assets/blob/master/LICENSE) указывают **Creative Commons Attribution 4.0 International (CC BY 4.0)**, если конкретная папка не содержит иной лицензии.

**Атрибуция: Babylon.js contributors / BabylonJS Assets.**

| Локальный файл | Исходный файл | Использование / изменение |
| --- | --- | --- |
| `outdoor.env` | [`environments/sanGiuseppeBridge.env`](https://github.com/BabylonJS/Assets/blob/master/environments/sanGiuseppeBridge.env) | Image-based lighting и отражения; без изменений |
| `dead-tree.glb` | [`meshes/graveYardPack/tree3/tree3.glb`](https://github.com/BabylonJS/Assets/blob/master/meshes/graveYardPack/tree3/tree3.glb) | Сухие деревья; масштаб и материал настраиваются в игре |
| `tree-billboard.png` | [`textures/tree.png`](https://github.com/BabylonJS/Assets/blob/master/textures/tree.png) | RGBA-листва на перекрестных плоскостях; ствол и ветви собираются процедурно |
| `bark.png` | [`textures/woodAlbedo.png`](https://github.com/BabylonJS/Assets/blob/master/textures/woodAlbedo.png) | Кора; UV-тайлинг и цвет настраиваются в материале |
| `grass-normal.png` | [`textures/grassn.png`](https://github.com/BabylonJS/Assets/blob/master/textures/grassn.png) | Normal-map травы; без изменений |
| `rock.png` | [`textures/rock.png`](https://github.com/BabylonJS/Assets/blob/master/textures/rock.png) | Albedo камня; без изменений |
| `rock-normal.png` | [`textures/rockn.png`](https://github.com/BabylonJS/Assets/blob/master/textures/rockn.png) | Normal-map камня; без изменений |
| `sand.jpg` | [`textures/sand.jpg`](https://github.com/BabylonJS/Assets/blob/master/textures/sand.jpg) | PBR-песок пустыни в «МИРАЖЕ» и берег в архивной игре; без изменений |
| `water-bump.png` | [`textures/waterbump.png`](https://github.com/BabylonJS/Assets/blob/master/textures/waterbump.png) | Bump/normal-map воды; без изменений |

Текст лицензии: <https://creativecommons.org/licenses/by/4.0/legalcode>. Права на торговые марки и логотипы этой атрибуцией не передаются.

## Fox — Khronos glTF Sample Assets

Локальный `fox.glb` взят из [Models/Fox/glTF-Binary/Fox.glb](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Fox/glTF-Binary/Fox.glb). Исходные уведомления: [README](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Fox/README.md) и [LICENSE.md](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Fox/LICENSE.md).

- модель © 2014 **PixelMannen**, **CC0 1.0 Universal**;
- риггинг и анимация © 2014 **tomkranis**, **CC BY 4.0**;
- конвертация в glTF © 2017 **@AsoboStudio и @scurest**, **CC BY 4.0**.

В игре сохранены скелет и анимации `Survey`, `Walk`, `Run`; для архетипов противников изменяются цвета/материалы, масштаб и добавляются эффекты.

## Soldier — Mixamo / three.js examples

Локальный `soldier.glb` взят без изменений из [three.js `examples/models/gltf/Soldier.glb`](https://github.com/mrdoob/three.js/blob/dev/examples/models/gltf/Soldier.glb). Пример Three.js [указывает Mixamo](https://github.com/mrdoob/three.js/blob/dev/examples/webgl_animation_skinning_blending.html) как источник модели.

**Атрибуция: модель и анимации — Mixamo; распространение в примере — three.js contributors.** Используется как встроенный игровой персонаж, а не как отдельно распространяемый стоковый ассет. Масштаб, материал и воспроизведение анимаций настраиваются игрой.

## Текстуры из three.js examples

| Локальный файл | Исходный файл |
| --- | --- |
| `grass-photo.jpg` | [`examples/textures/terrain/grasslight-big.jpg`](https://github.com/mrdoob/three.js/blob/dev/examples/textures/terrain/grasslight-big.jpg) |
| `wood.jpg` | [`examples/textures/hardwood2_diffuse.jpg`](https://github.com/mrdoob/three.js/blob/dev/examples/textures/hardwood2_diffuse.jpg) |
| `wood-bump.jpg` | [`examples/textures/hardwood2_bump.jpg`](https://github.com/mrdoob/three.js/blob/dev/examples/textures/hardwood2_bump.jpg) |
| `wood-roughness.jpg` | [`examples/textures/hardwood2_roughness.jpg`](https://github.com/mrdoob/three.js/blob/dev/examples/textures/hardwood2_roughness.jpg) |

**Атрибуция: three.js contributors.** Файлы используются в неизменённом виде как игровые текстуры; UV-тайлинг и PBR-параметры задаются в коде. Уведомления репозитория: [three.js LICENSE](https://github.com/mrdoob/three.js/blob/dev/LICENSE).

## Шрифты

Интерфейс использует **Manrope** и **Unbounded** через локально собранные пакеты Fontsource. Шрифты распространяются по **SIL Open Font License 1.1**; лицензии находятся в соответствующих npm-пакетах. WOFF2-файлы встраиваются в автономный HTML.

## Оригинальная часть

Игровой код, бесшовный рельеф пустыни и архивного острова, процедурные кактусы, агавы, руины, оазис, копьё, sentry и вышка связи, сборка деревьев, SVG-иконки, HUD, игровые системы и синтезируемый Web Audio-звук созданы для этого проекта. Внешние ассеты не выдаются за оригинальные модели автора проекта.
