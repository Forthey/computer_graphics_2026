# Структура Assets

В проекте используется прослойка `active` для ресурсов, которые предполагается часто менять. Да, это костыль, так как я сильно не хочу писать чтение путей откуда-либо (из CLA, например)

## Текстура куба

Путь, который использует программа:

- `Assets/cube/active.dds`

Чтобы переключить текстуру куба, нужно скопировать нужный `.dds` поверх `active.dds`

## Sky cubemap

Пути, которые использует программа:

- `Assets/sky/active/px.dds`
- `Assets/sky/active/nx.dds`
- `Assets/sky/active/py.dds`
- `Assets/sky/active/ny.dds`
- `Assets/sky/active/pz.dds`
- `Assets/sky/active/nz.dds`

Чтобы переключить skybox, нужно заменить содержимое `Assets/sky/active` шестью гранями из другого набора
