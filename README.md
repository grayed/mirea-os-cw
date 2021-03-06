# Материалы для курсовых по ОС/ОММвРОС в РТУ МИРЭА

Так как у разных курсов и специальностей постановка задач сильно отличается,
материал сгруппирован по типу курсовой работы.
Для начала работы следует ознакомиться с соответствующей документацией:

* [КММО-1-20](kmmo-X-20), [КММО-1-21](kmmo-X-21) — [Файловая система в ядре](kernelfs.md)

## Подготовка к работе

В случае использования платформы, поддерживаемой инфраструктурой данного репозитория,
для существенного облегчения подгтовительного этапа следует выполнить следующее
на машине, где будут вестись сборка и тестирование:

1. Клонировать данный репозиторий.
2. Зайти в появившуюся папку, а в ней — в папку, соответствующую потоку обучения,
   см. список выше.
3. При желании, настроить параметры сборки, см. ниже.
   В большинстве случаев параметры по умолчанию трогать не требуется.
4. Запустить первоначальную настройку и сборку, убедиться в отсутствии ошибок.

Ниже приведён минимальный набор команд для выполнения вышеозначенных действий:
```sh
git clone https://github.com/grayed/mirea-os-cw
cd mirea-os-cw
make
```

Для установки собранных компонентов требуются права суперпользователя, они
будут получены автоматически с помощью переменной `${DOAS}` (см. ниже).
Сама установка делается следующей командой:
```sh
make install
```

Подразумевается, что git, make и компилятор языка Си уже установлены в системе.
Если это не так, то нужно их установить. Для OpenBSD это потребует следующую команду:
```sh
pkg_add git
```
Для FreeBSD команда чуть-чуть отличается:
```sh
pkg add git
```
В *BSD компилятор Си и программа make входят в состав базовой системы и поэтому
обычно уже установлены по умолчанию.

В мире Linux команда и название пакетов для установки зависят от дистрибутива:
```sh
apt-get install gcc git make    # ALT, Debian, Ubuntu и производные
yum install gcc git make        # CentOS, Fedora, RHEL и производные
pacman -S gcc git make          # Arch и производные
```
В случае наличия нескольких версий того же GCC, следует ориентироваться на умолчания,
а не на самую новую или старую версию: сборка ядра порой ломается из-за неверного
выбора компилятора.

### Настройка параметров сборки

При желании можно настроить используемые каталоги и другие параметры сборки,
задав их с помощью переменных окружения или параметров командной строки make:

* `DOAS` — команда, используемая для выполнения других команд от имени суперпользователя,
   по умолчанию — `doas` в OpenBSD и `sudo` во FreeBSD.
* `FETCH_CMD` — команда, используемая для скачивания, должна по возможности осуществлять
   докачку не полностью скачанных файлов и поддерживать аргумент «-o outfile».
* `FETCH_DIR` — папка, в которую будут сохранены требуемые исходные тексты ОС,
   по умолчанию — папка вида `OpenBSD/X.Y` или `FreeBSD/X.Y-RELEASE`, в зависимости от ОС,
   в домашнем каталоге текущего пользователя.
* `FETCH_URI` — базовый URI, откуда можно скачать исходные тексты ОС;
* `KERNEL_CONFIG` — собираемая конфигурация ядра, по умолчанию —- `GENERIC`;
* `OBJ_DIR_`*компонент* — каталог, в котором будет собираться *компонент* ОС;
* `SRC_DIR_`*компонент* — каталог, где будет распакован *компонент* ОС.

Чтобы не указывать эти параметры каждый раз в командной строке, их можно прописать
в файле `local.mk` в папке вашего потока обучения.
Например, если вы являетесь студентом группы КММО-01-20, находитесь в корневой
папке репозитория и хотите, чтобы исходники ядра располагались в папке
`/usr/src/sys.mirea`, то достаточно выполнить следующую команду,
чтобы запомнить это значение на постоянной основе:

	```sh
	echo SRC_DIR_kernel = /usr/src/sys.mirea >>kmmo-X-20/local.mk
	```
