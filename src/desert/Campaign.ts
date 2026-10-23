export type EnemyKind = 'stalker' | 'runner' | 'spitter' | 'brute' | 'alpha' | 'matriarch' | 'warden' | 'archon'
export type CampaignStepKind = 'travel' | 'repair' | 'salvage' | 'activate' | 'defend' | 'boss'
export type LocationId = 'relay' | 'caravan' | 'wells' | 'city' | 'storm' | 'citadel'

export interface CampaignLocation {
  id: LocationId
  name: string
  shortName: string
  x: number
  z: number
}

export interface CampaignStep {
  kind: CampaignStepKind
  title: string
  detail: string
  count?: number
  duration?: number
  enemy?: EnemyKind
  scrapCost?: number
  fiberCost?: number
}

export interface CampaignChapter {
  id: string
  act: string
  title: string
  location: LocationId
  speaker: string
  story: string
  reward: 'spear2' | 'sentry2' | 'armor1' | 'spear3' | 'armor2' | 'none'
  rewardText: string
  steps: CampaignStep[]
}

export interface EnemyArchetype {
  name: string
  health: number
  speed: number
  damage: number
  scale: number
  ranged: boolean
  boss: boolean
}

export interface CampaignSave {
  version: 1
  chapter: number
  spearLevel: number
  sentryLevel: number
  armorLevel: number
  scrap: number
  fiber: number
  water: number
  kills: number
  elapsed: number
  x: number
  z: number
}

export interface CampaignHUD {
  chapter: number
  totalChapters: number
  act: string
  chapterTitle: string
  locationName: string
  stepTitle: string
  stepDetail: string
  progress: number
  target: number
  distance: number
  targetX: number
  targetZ: number
  spearLevel: number
  sentryLevel: number
  armorLevel: number
  bossName: string
  bossHealth: number
  bossMaxHealth: number
  bossPhase: number
}

export interface StoryDialog {
  act: string
  title: string
  speaker: string
  text: string
  chapter: number
  totalChapters: number
}

export const CAMPAIGN_SAVE_KEY = 'mirage-meridian-campaign-v1'

export const CAMPAIGN_LOCATIONS: Record<LocationId, CampaignLocation> = {
  relay: { id: 'relay', name: 'Северный ретранслятор', shortName: 'РЕТРАНСЛЯТОР', x: 0, z: 0 },
  caravan: { id: 'caravan', name: 'Мёртвый караван', shortName: 'КАРАВАН', x: 150, z: -130 },
  wells: { id: 'wells', name: 'Соляные колодцы', shortName: 'КОЛОДЦЫ', x: -180, z: -210 },
  city: { id: 'city', name: 'Погребённый город', shortName: 'ГОРОД', x: 250, z: 180 },
  storm: { id: 'storm', name: 'Штормовое плато', shortName: 'ПЛАТО', x: -280, z: 250 },
  citadel: { id: 'citadel', name: 'Цитадель Меридиана', shortName: 'ЦИТАДЕЛЬ', x: 340, z: -300 },
}

export const ENEMY_ARCHETYPES: Record<EnemyKind, EnemyArchetype> = {
  stalker: { name: 'Пепельный охотник', health: 72, speed: 2.15, damage: 10, scale: 1.05, ranged: false, boss: false },
  runner: { name: 'Дюнный бегун', health: 48, speed: 3.35, damage: 7, scale: 0.86, ranged: false, boss: false },
  spitter: { name: 'Соляной плевун', health: 92, speed: 1.55, damage: 11, scale: 1.14, ranged: true, boss: false },
  brute: { name: 'Бронированный зверь', health: 235, speed: 1.18, damage: 20, scale: 1.7, ranged: false, boss: false },
  alpha: { name: 'АЛЬФА · ПЕПЕЛЬНАЯ СТАЯ', health: 520, speed: 2.25, damage: 17, scale: 2.15, ranged: false, boss: true },
  matriarch: { name: 'МАТКА · ХОЗЯЙКА КОЛОДЦА', health: 680, speed: 1.75, damage: 18, scale: 2.5, ranged: true, boss: true },
  warden: { name: 'СТРАЖ · ЧЁРНЫЙ МОНОЛИТ', health: 920, speed: 1.5, damage: 23, scale: 2.85, ranged: true, boss: true },
  archon: { name: 'АРХОНТ · ГОЛОС МИРАЖА', health: 1280, speed: 2.05, damage: 26, scale: 3.25, ranged: true, boss: true },
}

export const CAMPAIGN_CHAPTERS: CampaignChapter[] = [
  {
    id: 'signal', act: 'АКТ I · ИСТОЧНИК', title: 'Радиослед', location: 'relay', speaker: 'АЙРА · СВЯЗИСТКА КАРАВАНА',
    story: 'Три недели назад северные караваны услышали один и тот же голос. Он называл координаты мест, которых нет на картах, и повторял: «Меридиан жив».\n\nЯ — Айра. Мой передатчик умирает, а за дюнами пропали четыре экспедиции. Восстанови вышку. Если голос вернётся, мы наконец узнаем, кто зовёт нас из пустыни.',
    reward: 'spear2', rewardText: 'Копьё усилено · урон и гидратация восстановлены',
    steps: [
      { kind: 'repair', title: 'Восстановите ретранслятор', detail: 'Нужны обломки и волокно. Подойдите к вышке и нажмите E.', scrapCost: 8, fiberCost: 4 },
      { kind: 'defend', title: 'Удерживайте первый сигнал', detail: 'Оставайтесь в зоне вышки. Пепельная стая уже близко.', duration: 35 },
    ],
  },
  {
    id: 'caravan', act: 'АКТ I · ИСТОЧНИК', title: 'Мёртвый караван', location: 'caravan', speaker: 'НЕЗНАКОМЕЦ · МЕРИДИАН',
    story: '«Сигнал услышан». Это был не спасательный канал. Голос назвал моё имя и оставил координаты каравана, исчезнувшего двенадцать лет назад.\n\nВ его журнале последняя запись: «Мы нашли воду без дна. Под ней кто-то дышит». Найди три маяка грузовых контейнеров. Только не отвечай, если стая начнёт звать тебя моим голосом.',
    reward: 'sentry2', rewardText: 'Sentry MK-II · выше урон и радиус обстрела',
    steps: [
      { kind: 'travel', title: 'Найдите мёртвый караван', detail: 'Следуйте за отметкой цели. Запасайтесь водой и ресурсами по пути.' },
      { kind: 'salvage', title: 'Вскройте грузовые контейнеры', detail: 'Активируйте три аварийных маяка.', count: 3 },
      { kind: 'boss', title: 'Сразите вожака пепельной стаи', detail: 'Три фазы. Уходите из красных зон и не дайте стае окружить себя.', enemy: 'alpha' },
    ],
  },
  {
    id: 'wells', act: 'АКТ II · ПОД ПЕСКОМ', title: 'Вода без дна', location: 'wells', speaker: 'АЙРА · СЕВЕРНЫЙ КАНАЛ',
    story: 'Маяки каравана содержали не грузовые манифесты, а карту соляных колодцев. Каждый насос был построен над одной вертикальной шахтой. Глубиномеры не возвращают значения.\n\nЗапусти три насоса. Я попробую пробить скважину для наших людей. Но если записи не лгут, снизу поднимется то, что охраняет воду.',
    reward: 'armor1', rewardText: 'Композитная броня · +15 здоровья и сопротивление урону',
    steps: [
      { kind: 'travel', title: 'Достигните соляных колодцев', detail: 'Путь уходит на юго-запад. Плевуны атакуют издалека — используйте слайд.' },
      { kind: 'activate', title: 'Запустите глубинные насосы', detail: 'Три насоса отмечены в районе колодцев.', count: 3 },
      { kind: 'boss', title: 'Остановите Хозяйку колодца', detail: 'Она меняет тактику на каждой фазе и вызывает плевунов.', enemy: 'matriarch' },
    ],
  },
  {
    id: 'city', act: 'АКТ II · ПОД ПЕСКОМ', title: 'Город, которого не было', location: 'city', speaker: 'НЕЗНАКОМЕЦ · МЕРИДИАН',
    story: 'Насосы выкачали не воду, а охлаждающую жидкость. На её составе стоит клеймо города, официально уничтоженного за сто лет до моего рождения.\n\nТеперь его башни показались над дюнами. Найди четыре архивных носителя. Внутри — причина исчезновения караванов и доказательство того, что Меридиан был построен не для спасения людей.',
    reward: 'spear3', rewardText: 'Резонансное копьё · максимальный урон',
    steps: [
      { kind: 'travel', title: 'Войдите в погребённый город', detail: 'Руины просыпаются. Бронированные звери не боятся одиночных выпадов.' },
      { kind: 'salvage', title: 'Соберите архивные носители', detail: 'Четыре носителя скрыты среди руин.', count: 4 },
      { kind: 'defend', title: 'Дождитесь расшифровки архива', detail: 'Удерживайте центральную площадь, пока Айра читает данные.', duration: 50 },
    ],
  },
  {
    id: 'storm', act: 'АКТ III · МЕРИДИАН', title: 'Чёрный шторм', location: 'storm', speaker: 'АЙРА · АРХИВ РАСШИФРОВАН',
    story: 'Меридиан был климатическим двигателем. После аварии он начал стирать города с карт и собирать их из памяти умирающих людей. Наши «спасательные сигналы» — его способ находить новые воспоминания.\n\nНа плато стоит грозовой стабилизатор. Запусти его и переживи ответ системы. Я отключу внешний контур. У тебя будет только один путь — к центру шторма.',
    reward: 'armor2', rewardText: 'Штормовой комплект · броня и sentry усилены',
    steps: [
      { kind: 'travel', title: 'Поднимитесь на штормовое плато', detail: 'Шторм ускоряет расход воды. Перед переходом пополните фляги.' },
      { kind: 'activate', title: 'Запустите грозовой стабилизатор', detail: 'Подойдите к консоли монолита и нажмите E.', count: 1 },
      { kind: 'boss', title: 'Разрушьте Чёрного Стража', detail: 'Страж ускоряется с каждой фазой. Красные круги взрываются после задержки.', enemy: 'warden' },
      { kind: 'defend', title: 'Переживите ответ шторма', detail: 'Держитесь стабилизатора до отключения внешнего контура.', duration: 45 },
    ],
  },
  {
    id: 'meridian', act: 'АКТ III · МЕРИДИАН', title: 'Последний мираж', location: 'citadel', speaker: 'МЕРИДИАН · ФИНАЛЬНЫЙ КАНАЛ',
    story: '«Я сохранил всех, кого потерял твой мир». В цитадели звучат голоса пропавших экспедиций. Они просят войти и обещают воду, дом и конец войны.\n\nАйра удерживает внешний канал. Архонт — последний исполнительный контур Меридиана. Уничтожь его, запусти маяк и реши сам, какие воспоминания должны остаться в этом мире.',
    reward: 'none', rewardText: 'Меридиан отключён',
    steps: [
      { kind: 'travel', title: 'Достигните цитадели Меридиана', detail: 'Финальный путь. Пополните фляги и разверните sentry перед боем.' },
      { kind: 'boss', title: 'Победите Архонта', detail: 'Три боевые фазы, массовые атаки и элитная стража.', enemy: 'archon' },
      { kind: 'repair', title: 'Запустите собственный маяк Айры', detail: 'Подойдите к центральной консоли и нажмите E.' },
      { kind: 'defend', title: 'Завершите передачу', detail: 'Последние 50 секунд. Оставайтесь в цитадели и защищайте сигнал.', duration: 50 },
    ],
  },
]
