# Gameplay Knowledge Base

Документ фиксирует базовую модель боя для следующего слоя gameplay core. Это не
финальный баланс и не art direction. Цель - сделать правила оружия, стихий,
статусов, реакций, врагов и боссов достаточно ясными, чтобы их можно было
реализовать поверх текущих `GameSession`, `RoomGraph` и `CombatSim` без повторного
пересбора концепции.

## 1. Базовый словарь

`Actor` - любой участник боя: игрок, обычный враг, элитный враг или босс. Игрок,
враги и боссы должны проходить через один и тот же damage/status pipeline.

`WeaponSlot` - слот оружия у Actor. У игрока в v0 закладываются 3 слота, но
активен только один. Враги и боссы могут иметь меньше слотов, но используют тот
же формат.

`WeaponSpec` - данные оружия: категория, базовый урон, скорость применения,
привязанная стихия, две доступные кнопки действия, procedural visual tag.

`WeaponAction` - действие активного оружия на одной из двух кнопок. У любого
оружия есть `action1` и `action2`; что именно они делают, задается данными
оружия.

`AttackSpec` - описание формы атаки: источник, направление, область, дальность,
скорость, длительность, элемент, damage intent и список hit rules.

`DamageEvent` - единая точка входа для урона. Любой удар игрока, врага или босса
создает `DamageEvent`; через него считаются урон, щиты, базовая стихия, статусы и
комбинации.

`StatusInstance` - активный статус на Actor. На цели может быть максимум один
статус каждого элемента. Повторное наложение того же статуса обновляет его и
выдает новый `instanceId`.

`ReactionRule` - направленное правило комбинации `existing status + incoming
element`. Порядок важен: вода + огонь и огонь + вода могут иметь разные свойства.

`ShieldState` - будущий слой щитов. В v0 он описан как hook: камень игнорирует
щиты, лед + камень повышает эффективность снятия щитов.

`BossPhase` - фазовый слой поверх обычного Actor. Босс остается Actor, но фаза
может менять доступные атаки, AI-pattern, сопротивления и scripted mechanics.

## 2. Общий боевой pipeline

1. Actor выбирает активный `WeaponSlot`.
2. Нажатая кнопка активного оружия выбирает `WeaponAction`.
3. `WeaponAction` создает один или несколько `AttackSpec`.
4. При попадании `AttackSpec` создает `DamageEvent`.
5. `DamageEvent` делает snapshot всех активных статусов цели до изменения цели.
6. Входящий элемент реагирует со всеми активными статусами из snapshot.
7. Все процентные модификаторы урона от реакций складываются суммой процентов.
8. Урон проходит через `ShieldState`; каменный урон игнорирует щиты.
9. Базовый эффект входящего элемента и side effects реакций применяются фазами:
   removals, additions/refresh, locks/stuns.
10. Если хотя бы одна реакция создала `StatusLock`, все новые статусы из этого же
    `DamageEvent` подавляются.
11. Из pipeline выходят data-only события для будущего UI, VFX, звука и
    аналитики баланса.

Проценты урона считаются от текущего значения удара. Например, если две реакции
дают `+20%` и `-10%`, итоговый множитель будет `+10%`.

## 3. Actor model

Каждый Actor должен хранить:

- позицию, скорость, room index, hp и death state;
- faction: player, enemy, boss, neutral/summon;
- 3 weapon slots как максимум для игрока, active slot index;
- active status slots: `Wet`, `Burning`, `Charged`, `Chilled`;
- future `ShieldState`;
- future upgrade modifiers: damage, attack speed, cooldowns, dash, element power,
  reaction power, resistance, boss phase modifiers.

Камень и воздух не создают persistent status в v0. Если будущий босс или предмет
создаст forced stone/air status, для него по умолчанию действует no-op строка
матрицы реакций.

## 4. Weapon model

Оружие должно быть данными, а не отдельной боевой системой. Все weapon actions
создают `AttackSpec`, а дальше работают через общий `DamageEvent`.

Все оружие при создании получает одну стихию. Эта стихия применяется ко всем
damage events оружия, если конкретная будущая атака явно не переопределит элемент.

### Магическое оружие

Магическое оружие не имеет basic attack. Обе кнопки являются spell actions.

Примеры категорий:

- посох: контролируемые spell patterns на дистанции;
- скипетр: более направленные и мощные spell casts;
- перчатки: ближняя/средняя магия вокруг персонажа.

Примеры действий:

- орбитальные круги на расстоянии от персонажа, которые наносят урон при касании;
- метеорит или targeted area cast в выбранную область;
- зона контроля вокруг игрока;
- медленный piercing projectile.

### Дальний бой

Патронов нет. Ограничители дальнего боя - fire rate, spread, reload-like
cooldowns, self-root, range и special cooldown.

- Пистолет:
  - `action1`: одиночные точные выстрелы;
  - `action2`: заряженный медленный большой снаряд.
- Винтовка:
  - `action1`: точный дальний выстрел;
  - `action2`: короткая серия выстрелов во все стороны, стартово 10 направлений.
- Пулемет:
  - `action1`: частая стрельба вперед;
  - `action2`: Actor останавливается, резко повышает скорострельность и стреляет
    конусом.
- Дробовик:
  - `action1`: конус с ограниченной дистанцией;
  - `action2`: короткая shockwave-конус атака с сильным отталкиванием и
    ограниченной дистанцией.

### Ближний бой

Ближний бой наносит высокий урон, но требует позиции и риска.

- Молот:
  - `action1`: прыжковый удар по области;
  - `action2`: раскрутка вокруг себя.
- Копье:
  - `action1`: прямой укол вперед;
  - `action2`: dash вперед на копье с неуязвимостью во время движения.
- Катана:
  - `action1`: рубящие удары с площадью больше, чем у копья;
  - `action2`: дуга-волна вперед. После каждого третьего применения дуга
    дублируется в месте, где была нажата способность, а не в текущей позиции
    героя.

## 5. Elements and statuses

| Element | Persistent status | Base behavior |
| --- | --- | --- |
| Вода | `Wet` | Накладывает мокрый статус. Сам по себе статус не наносит урон, но является сильным катализатором реакций. |
| Огонь | `Burning` | Накладывает горение. Горение наносит урон как процент от текущего HP цели. |
| Камень | Нет | Не накладывает статус. Каменный урон игнорирует щиты. |
| Электричество | `Charged` | Накладывает заряд. Заряженные цели могут разряжаться между собой. Один `instanceId` не должен повторно прокать тот же уникальный разряд. |
| Лед | `Chilled` | Накладывает подмороженность и замедляет движение цели на процент от ее скорости. |
| Воздух | Нет | Не накладывает статус. Отталкивает цель от точки получения урона. |

Status refresh rule: если на Actor уже есть статус того же элемента, новый hit
обновляет duration/intensity по правилам эффекта и назначает новый `instanceId`.

Multi-status rule: Actor может одновременно держать несколько разных статусов,
например `Wet + Chilled + Charged`.

Status lock rule: пока на цели активен `StatusLock`, новые статусы не
накладываются и не refresh-ятся. Урон и non-status эффекты продолжают работать.

## 6. Reaction matrix

Формат: `existing status/catalyst + incoming element`.

`No persistent status` означает, что строка невозможна в обычном v0 gameplay,
потому что камень и воздух не оставляют статуса. Если будущая boss mechanic
принудительно создаст такой статус, все клетки строки считаются no-op, а входящий
элемент применяет base behavior.

| Existing / Incoming | Вода | Огонь | Камень | Электричество | Лед | Воздух |
| --- | --- | --- | --- | --- | --- | --- |
| Вода / `Wet` | Refresh `Wet`. | Урон огня `-50%`; снять `Wet`; подавить `Burning`; после реакции нет статусов от этой пары. | Нет спецэффекта; `Wet` остается; каменный урон игнорирует щиты. | Урон электричества `+20%`; создать stun sequence: следующие N секунд раз в секунду оглушение на 0.1 сек; наложить `StatusLock`; после окончания не остается статусов от этой пары. | Урон льда `+20%`; усилить slow еще на `+20%` к базовому льду; применить/refresh `Chilled`; эффект может продлеваться водой или льдом. | Снять `Wet` с цели; распространить `Wet` на соседей в области, которые не получают этот же air hit и не получали air damage последние 0.1 сек; применить air knockback. |
| Огонь / `Burning` | Урон воды `-10%`; снять `Burning`; подавить `Wet`; после реакции нет статусов от этой пары. | Refresh/extend `Burning`. | Продлить `Burning`; продление камнем может повторяться бесконечно; каменный статус не накладывается. | Микровзрыв: дополнительный area damage равен `30%` от incoming electric damage; снять `Burning` и `Charged` со всех целей, попавших во взрыв. | Заменить реакцию на `Wet`: снять `Burning`, подавить `Chilled`, применить/refresh `Wet`; урон не меняется. | Усилить текущий `Burning` на `+50%` до его окончания; duration не продлевать; применить air knockback. |
| Камень / no persistent status | No-op; применить base water behavior. | No-op; применить base fire behavior. | No-op; применить base stone behavior. | No-op; применить base electric behavior. | No-op; применить base ice behavior. | No-op; применить base air behavior. |
| Электричество / `Charged` | Тот же эффект, что `Wet + Electricity`, но модифицирует incoming water damage: `+20%`; создать stun sequence и `StatusLock`; после окончания не остается статусов от этой пары. | Тот же микровзрыв, что `Burning + Electricity`, но source damage берется от incoming fire damage; снять `Burning` и `Charged` со всех целей во взрыве. | Нет спецэффекта; `Charged` остается; каменный урон игнорирует щиты. | Refresh `Charged`; новый `instanceId` сбрасывает уникальность будущих discharge checks. | Нет спецэффекта; оставить `Charged`; применить/refresh `Chilled`. | Нет спецэффекта; `Charged` остается; применить air knockback. |
| Лед / `Chilled` | Тот же эффект, что `Wet + Ice`, но модифицирует incoming water damage: `+20%`; усилить slow; применить/refresh `Wet` и сохранить/refresh `Chilled`. | Тот же результат, что `Burning + Ice`: снять `Chilled`, подавить `Burning`, применить/refresh `Wet`; урон не меняется. | Эффективность снятия щитов `+50%`; `Chilled` остается; каменный статус не накладывается. | Нет спецэффекта; оставить `Chilled`; применить/refresh `Charged`. | Refresh `Chilled`. | Нет спецэффекта; `Chilled` остается; применить air knockback. |
| Воздух / no persistent status | No-op; применить base water behavior. | No-op; применить base fire behavior. | No-op; применить base stone behavior. | No-op; применить base electric behavior. | No-op; применить base ice behavior. | No-op; применить base air behavior. |

## 7. Electric discharge rule

`Charged` имеет дополнительную логику вне основной матрицы:

- если рядом есть несколько Actor с `Charged`, между ними может возникнуть
  discharge;
- discharge наносит процент от damage оружия всем противникам между заряженными
  целями;
- один и тот же `Charged.instanceId` может прокнуть один раз на уникальную
  связь/цель;
- discharge не снимает `Charged`, если конкретная реакция не сказала снять
  электрические эффекты.

Точные радиусы, line test, cooldown discharge и процент урона - balance
placeholder.

## 8. Enemy and boss rules

Враги используют те же `WeaponSpec`, `WeaponAction`, `AttackSpec`, `DamageEvent`,
`StatusInstance` и `ReactionRule`, что игрок. AI не получает отдельные правила
урона: он только выбирает action активного оружия.

Враг может иметь:

- один или несколько weapon slots;
- активную стихию оружия;
- простую AI-логику выбора дистанции и action;
- resist/vulnerability hooks в будущем.

Босс моделируется как `Actor + BossPhase`.

`BossPhase` может менять:

- active weapon или набор scripted `AttackSpec`;
- pattern выбора actions;
- дополнительные resistances или vulnerability windows;
- правила появления adds;
- room objective pressure;
- scripted elemental mechanics.

Босс не должен обходить общий damage/status pipeline. Даже уникальная механика
босса должна в итоге создавать `DamageEvent` или явно документированный
non-damage event.

## 9. Upgrade hooks

Прокачки пока не реализуются, но дизайн должен оставлять hooks:

- weapon damage;
- weapon attack speed / cooldown;
- special cooldown;
- projectile speed, area size, cone angle, duration;
- element power;
- отдельная reaction power для конкретных пар;
- dash cooldown, dash distance, invulnerability duration;
- actor max HP, movement speed, resistance;
- shield strength и shield recovery;
- boss phase modifiers.

Эти hooks не должны менять базовые правила pipeline. Они только модифицируют
параметры `WeaponSpec`, `AttackSpec`, `DamageEvent`, `StatusInstance` или
`ReactionRule`.

## 10. Future CPU-only test scenarios

Будущая реализация должна покрыть deterministic tests:

- Actor может держать несколько разных статусов одновременно.
- Incoming element реагирует со всеми активными статусами snapshot-а.
- Повторное наложение того же статуса refresh-ит один status slot и меняет
  `instanceId`.
- `Wet + Fire` уменьшает урон огня, снимает воду и не оставляет статусов.
- `Burning + Water` уменьшает урон воды, снимает огонь и не оставляет статусов.
- `Wet + Electricity` создает stun sequence и `StatusLock`.
- `Charged` discharge не повторяется для одного `instanceId`.
- `Wet + Air` переносит `Wet` на соседей по правилу air-hit exclusion.
- 3 weapon slots существуют, но actions читает только active weapon.
- Magic weapon имеет две spell actions и не имеет basic attack.
- Enemy damage events проходят через тот же pipeline, что player damage events.
- Boss phase attack создает обычный `DamageEvent` и участвует в реакциях.

## 11. Boundaries

В этом документе не фиксируются точные duration, cooldowns, HP, DPS, spawn
density, радиусы, цены улучшений, UI, VFX, звук, лут, инвентарь,
meta-progression и финальное визуальное направление.

Все конкретные числа, кроме уже названных относительных процентов, считаются
balance placeholder.
