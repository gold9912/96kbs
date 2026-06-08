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
- v0 enemy shield value; stone bypasses shield absorption, and chilled+stone can
  strip shield before HP damage.
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

Current v0 reward upgrades implement the first playable subset of these hooks:
`Damage` scales outgoing player action damage, `Cooldown` scales Q/E cooldowns,
`Speed` scales player movement/dash movement, `Area` scales melee/ring reach and
player projectile radius/lifetime, `MaxHp` increases max/current HP, and `Heal`
restores current HP. These are covered by deterministic gameplay tests that cast
real weapon actions rather than only inspecting stored stats.

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

## 10. v0 playable slice

Implementation note: the current playable loop is now
`room -> clear objective -> reward choice -> portal -> next room`. A completed
non-start, non-final room enters `RunPhase::RewardChoice`, exposes exactly three
deterministic `RewardOption` records, freezes combat progression, and opens
portals only after `InputState.rewardChoice` selects a valid option. Reward
options are data-only and can represent a weapon swap, an element infusion, or a
player upgrade. Each option includes an `iconSeed` so renderer-side procedural
icons can stay deterministic without external assets.

Current combat resolver contract: the original elemental sketch is treated as
the gameplay source of truth. A weapon action produces an elemental damage
event; the incoming element reacts against a snapshot of all target statuses
before any removals or additions mutate the actor. Reaction damage modifiers are
summed as percentages, reaction removals/status-locks are applied before new
statuses, and player/enemy hits use the same resolver. The current implemented
slice covers multi-status `Wet + Chilled`, boosted slow for `Wet + Ice` /
`Chilled + Water`, `Wet + Fire`, `Burning + Water`, `Charged + Water`,
`Wet + Air`, `Burning + Electricity` / `Charged + Fire` micro explosions,
shared enemy-to-player status application, stone shield bypass, chilled+stone
shield stripping, air knockback hooks including `Charged + Air` /
`Chilled + Air` readable reaction events, and `Charged` discharge between nearby
charged enemies.
Boosted ice/water reactions raise `Chilled` intensity by `+0.20` over the base
slow and preserve the stronger value on refresh. Wet/electric and charged/water
reactions start a short status lock with an immediate stun pulse and a
deterministic follow-up pulse through the same `StunApplied` event stream.
Micro explosions are explicit area-reaction damage events:
they damage targets in a compact radius and remove `Burning` / `Charged` from
every affected target instead of hiding the effect as single-target bonus
damage. If the player is the reacting actor, the same micro-explosion contract
uses the player position as the center and damages nearby enemies through the
same reaction event stream. Discharge damages enemies on the short lane between
charged targets, does not remove `Charged`, and is gated by the charged status
`instanceId`: the same unique application cannot discharge the same link twice,
while a status refresh creates a new `instanceId` and can discharge again.

Input binding contract: `Q` and left mouse button are the primary weapon action
(`action1`/primary attack). `E` is the ability action (`action2`), with right
mouse button mirroring it for mouse-first play. Legacy `melee`, `ranged`, and
`control` booleans remain compatibility aliases driven by the same binding
helper, so tests and Win32 input cannot drift apart. Number keys `1`, `2`, and
`3` feed both weapon slot selection and reward selection; the active
`GameSession` phase decides which meaning is consumed. Movement and screen aim
bindings also live in shared helpers: `W/S` map to positive/negative world
up/down, `A/D` map left/right, and cursor positions above the viewport center
produce positive aim Y. This keeps the playable build and deterministic tests
aligned against inverted-control regressions.

Action readability contract: every `WeaponSpec` exposes short readable names for
`action1` and `action2`. The current starter loadout is `Katana` active
(`Q Slash`, `E Wave`), `Pistol` in slot 2, and `Gloves` in slot 3 as the clear
radial test weapon (`Q Pulse`, `E Field`). Runtime feedback must show the active
weapon, both action names, and the `Q`/`E` readiness state. In-world procedural
DXR feedback keeps only a small player-facing indicator during ready state.
Weapon name, action names, active element/slot, and real-time `Q`/`E` cooldown
percentages belong in the procedural DXR composite HUD, not in world-space
rings. The HUD also publishes each button's data-driven `AttackShape` as a
compact procedural badge and short label next to the action name, so `Q`/`E`
readability covers what the action does, not only whether it is ready. The same
HUD includes a compact three-slot loadout strip for keys `1`/`2`/`3`, with each
slot's weapon code, element color, active highlight, and mini Q/E readiness bars,
so reward swaps and weapon selection remain readable during combat. The same HUD
also shows the active room objective (`KILL`, `SURV`, or `CTRL`) plus a
compact progress percentage, so survival/control rooms read as live objectives
instead of a frozen combat state. The player status line belongs in
the same overlay as compact `Wet`/`Burning`/`Charged`/`Chilled` badges, and the
last elemental reaction should flash briefly as an `RX` code when the shared
damage/status pipeline emits `ReactionTriggered`. The HUD should read as a
lightweight visual system: shader-generated glass panel, element swatch, small
bitmap glyphs, status/reaction badges, and segmented cooldown bars, with no
external fonts, images, textures, or UI meshes. Run-end states (`DOWN` for
death and `CLEAR` for floor completion) are part of the same overlay contract,
not a separate debug/title-only path, so victory and failure never read as a
frozen combat screen. Persistent large
attack-footprint guides stay hidden, and action shape readability should come
from short-lived VFX, overlay state, and weapon behavior. `WeaponActionUsed`
events carry the weapon, element, action index, and `AttackShape`; `GameSession`
forwards that shape through `PlayerActionUsed`/`PlayerAbilityUsed` so the app can
show a transient cone, line, ring, or burst footprint without re-deriving combat
meaning in renderer code.

Implemented action-shape semantics: `Dash` moves the player along the aimed
traversable lane, damages enemies on that lane, and grants its short invulnerable
window; `TargetArea` resolves around the aimed point at weapon range instead of
the player's feet; `Orbit` resolves as an annular band around the player rather
than a plain point-blank circle. These rules keep the weapon roster data-driven
while making `Spear E`, `Scepter Q`, and `Staff Q` read as distinct actions.
The deterministic `full weapon roster runtime actions` test casts Q and E for
every `WeaponId`, verifies the emitted weapon/action/shape payload, and checks
projectile, dash, rooted, and cooldown side effects so the roster cannot degrade
into inert data.

Reward readability contract: reward phase title text lists all three choices,
and the procedural DXR composite HUD renders three reward cards from packed
`RewardOption` data. Each card must show the keyboard choice, reward type
(`WPN`, `INF`, `UPG`), target slot, and the relevant weapon/element/upgrade name.
Each in-world reward icon carries one, two, or three small procedural pips to
match the keyboard choice. Choosing `1`, `2`, or `3` must unlock progression and
emit `RewardSelected` without requiring debug/test-only input. Reward generation
must avoid no-op choices: weapon swaps cannot exactly match the current target
slot loadout, element infusions must change that slot's element, and full-health
players should not roll a pure heal as their upgrade. Weapon swaps should prefer
weapons outside the current three-slot loadout, and element infusions should
prefer elements not already equipped, so a reward offer increases build variety
instead of only repainting existing choices. Weapon-swap rewards should preserve
the replaced slot's broad combat role when an unused same-category weapon exists,
so a ranged slot usually remains a ranged role and a magic slot remains a spell
role. Element choices are not pure random diversity: weapon swaps and infusions
should prefer elements that form at least one directed reaction pair with another
currently equipped slot, and the two elemental reward cards should avoid offering
the same element when another synergistic choice is available. Each elemental
reward records a `synergyElement` from the current loadout when a directed
reaction pair exists; the HUD renders that as a compact `RX` link between the
existing element and the offered element. Reward cards must show enough combat
meaning before selection: weapon swaps include Q/E action names, and infusions
name the weapon they modify plus the new element. `RewardSelected` carries the
selected option's weapon/element payload plus the packed reward-card record, so
VFX/HUD code can show a short `GAIN` toast after selection without re-reading
stale reward cards. Applying any valid non-final reward also grants a small
room-clear recovery tick, so the player reads reward choice as a short reset beat
before the next pressure room rather than as a pure menu pause.
Deterministic `reward selection recovery reset beat` coverage first receives
real enemy damage, rejects an invalid choice without healing or opening a portal,
then verifies valid reward recovery, payload emission, portal opening, and next
room unlock.

Room completion contract: when a non-kill objective such as survival or control
completes, remaining room pressure despawns before reward choice begins. The
player should read this as a reward pause, not as combat freezing with live
enemies still on screen. `ControlPoint` is the procedural control objective:
the room owns a deterministic `controlPoint`, `controlRadius`, and hold
duration; progress increases while the player stands inside the marked zone and
decays slightly outside it. Killing enemies alone must not complete this
objective. Deterministic coverage must include a
full-floor route: every non-final room completes, offers exactly three choices,
accepts a `1`/`2`/`3` reward selection, opens the forward portal, unlocks the
next room, and the final room emits `FloorCompleted`.
After `FloorCompleted`, `advanceFloor` starts the next deterministic floor
instead of restarting the whole build. The transition increments
`floorIndex`, regenerates `RoomGraph`, resets short combat transients
(cooldowns, projectiles, statuses, active action feedback), places the player in
the new start room, keeps the first combat room available, and preserves the
player's loadout plus upgrade scalars. The Win32 runtime maps this to Enter.

Playable onboarding contract: the first combat room must be clearable through
ordinary input rather than test-only damage. A deterministic smoke player enters
room 1, selects the starter ranged slot, aims at live enemies, kites room
pressure, uses both `Q` primary and `E` ability actions, deals real combat
damage, reaches reward choice with three options, and keeps a readable survival
buffer. During that live fight the procedural overlay must track the active
weapon slot, active enemy pressure, and non-ready `Q`/`E` cooldown percentages;
world-space cooldown rings remain hidden.
The same smoke path continues through a keyboard reward selection into room 2:
reward choice must open the next portal, resume exploration, enter the survival
room, show live survival progress in the overlay, use real Q/E combat input
against live pressure, complete the timer objective, and pause at a second
three-choice reward without invoking test-only damage.
The playable path then selects another reward and enters room 3's control
objective. The player must move into the procedural zone, hold it with ordinary
input while still firing Q/E at live pressure, see live `CTRL` progress in the
overlay, see generated marker geometry at the control point, complete the
objective, and pause at a third three-choice reward. The full playable smoke
continues from that state into room 4 and the final room with the same real
movement, aim, Q, E, and keyboard reward input path. Room 4 must clear without
test-only damage at a readable TTK, then the final room must render boss pressure,
publish boss HP percent and phase in the procedural overlay, push the boss
through phase 2 and phase 3, emit `FloorCompleted`, and keep the player alive
through the victory state.

Enemy pressure contract: generated combat rooms now cycle Brute, Caster,
Skirmisher, and Bulwark archetypes and increase spawn pressure toward the late
rooms while staying inside `kMaxSpawns`. Every archetype must keep a readable
procedural RT silhouette and data-only weapon behavior.
Pressure is scored by archetype cost rather than raw spawn count: Brute and
Skirmisher are light pressure, Caster and Bulwark are heavier role pressure, and
Boss is final-room pressure. The generated floor uses deterministic pressure
budgets `7 -> 8 -> 10 -> 12 -> 17` for rooms 1 through final, and spawn positions
are spread away from room center so room entry gives the player a readable beat
before enemies collapse into range.
Active enemies also expose procedural HP, shield, and status readability through
RT proxies (`EnemyHealthBack`, `EnemyHealthFill`, `EnemyShieldFill`,
`EnemyStatusPip`). These markers are deterministic and no-external-asset, and
covered by the `enemy readability rt proxies` test so weapon damage, shields,
and elemental statuses are visible without debug-only world text.
Threat tells are also data-driven: the renderer asks for `EnemyAttackIntent`,
which carries the same weapon, element, action index, action shape, range, and
cooldown readiness used by enemy attacks. Ready in-range Brutes/Boss target
areas produce ring tells, projectile/wave threats produce line tells, and cone
pressure produces cone tells; out-of-range or freshly-fired enemies produce no
tell. This keeps enemy pressure readable without large permanent arrows or
duplicated renderer-side combat rules.

Implemented enemy pressure roles:

- Brute is the melee anchor: `Hammer` + `Stone`, action 1, direct area pressure.
- Caster is the ranged status source: `Staff` + `Electricity`, action 2,
  enemy-owned projectile, keeps distance before firing.
- Skirmisher is the fast harasser: `Katana` + `Air`, action 2 wave projectile,
  then a short backstep so the attack reads as hit-and-run pressure.
- Bulwark is the slow shield pressure: `Shotgun` + `Ice`, action 2 cone,
  direct chilled pressure at close-mid range.
- Boss is the final-room pressure actor: the last room is forced to `KillAll`
  and receives one deterministic boss spawn at the room center. Boss is still an
  ordinary combat actor with HP, shield, statuses, cooldowns, `WeaponSpec`, and
  `DamageEvent` flow. Its phase is derived from HP ratio: phase 1 uses fire
  `Scepter` target-area pressure, phase 2 uses electric `Staff` projectile
  pressure, and phase 3 uses ice `Shotgun` cone pressure. Each phase must emit a
  normal `WeaponActionUsed` event and feed the same player damage/status
  pipeline as other enemies.

Enemy projectiles use the same projectile state and event stream as player
projectiles, but with `ownerFaction = Enemy`. On hit they call the shared
player damage/status pipeline and emit `ProjectileHit` against `Faction::Player`.
`PlayerDamaged` game events must preserve the source `weapon` and `element`
payload so HUD/VFX can explain incoming pressure. The current overlay contract
uses that payload for a short damage flash and keeps a deterministic test for
event payload plus visible HP loss.

Ближайший playable-срез фиксируется как `room -> clear -> portal -> next room`.
Игрок может двигаться только внутри комнаты, через открытый портальный путь или
в unlocked комнату, соединенную открытым порталом. Закрытые порталы и locked
комнаты должны блокировать traversal на уровне gameplay, а не только визуально.

В v0 текущие melee/ranged/control действия остаются временными actions поверх
`CombatSim`. Они должны выпускать data-only события вроде `EnemyDamaged`,
`EnemyKilled`, `PlayerDamaged`, `RoomCompleted` и `PortalOpened`, чтобы UI/VFX/звук
могли реагировать без привязки к будущей полной `DamageEvent` реализации.

DXR-визуал этого слоя обязан процедурно показывать active room, readable enemies,
control objective marker, hit sparks, room clear pulse и portal-open pulse без внешних ассетов. Large arrow
telegraphs are intentionally avoided; the player keeps only a small facing
indicator, while cooldowns are shown in the screen overlay rather than as
world-space rings or persistent attack guides.

## 11. Future CPU-only test scenarios

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

## 12. Boundaries

В этом документе не фиксируются точные duration, cooldowns, HP, DPS, spawn
density, радиусы, цены улучшений, UI, VFX, звук, лут, инвентарь,
meta-progression и финальное визуальное направление.

Все конкретные числа, кроме уже названных относительных процентов, считаются
balance placeholder.
