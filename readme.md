# STM32F4xx: Пример передачи данных из DMA в порт GPIO

## Задача

1. При поступлении сигнала на пин PC4, запустить генерацию сигнала из памяти в GPIO с тактированием от таймера TIM1 по четвёртому каналу с синхронизацией по событию PWM.
2. Частота семплирования сигнала — 10 МГц.
3. Использовать регистр BSRR GPIOE и настроить на выход пины PE11 и PE12.
4. Софтварно запустить передачу данных на настроенные пины.

*Примечание 1*: Можно было бы использовать режим One Short Pulse, но `Repetition Counter (RCR - 8 bits value) must be between 0 and 255.`, а количество точек в массиве — 16636.

*Примечание 2*: Использовалась тестовая плата [STM32F4XX M](https://stm32-base.org/boards/STM32F407VET6-STM32F4XX-M#TFT-header) с микроконтроллером [STM32F407VGT6](https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html).

## Введение

В этом примере используется HAL. Софтварный запуск передачи по EXTI (по функции обратного вызова GPIO).

Оптимальная скорость передачи для STM32F4xx на AHB составляет около 10 МГц. DMA передаёт данные на GPIO через регистр BSRR, позволяя устанавливать или сбрасывать отдельные пины порта. Скорость передачи данных напрямую зависит от частоты срабатывания DMA. Если не использовать таймер, частота будет определяться системным тактированием и задержками самого DMA.

### Ограничения DMA:

Максимальная частота работы DMA ограничена его архитектурой. Для STM32F4xx теоретический предел составляет около 12-15 миллионов операций в секунду для потоковой передачи данных. На практике это зависит от системной шины (AHB), размера данных и конкуренции за доступ к памяти, поэтому лучше тактировать передачу при помощи таймера, иначе частота семплирования будет «плавать».

## Настройки

### В CubeMX

#### Настройка системного таймера

1. HCLK настроен на 100 МГц.
   ![Настройки SysClock](images/sysclock.png)
2. В TIM1 активируется 4 канал PWM (без генерации сигнала).
3. Прескалер 0, счетчик 10-1. Таким образом, получится частота 10МГц на порту GPIO.

![Настройки TIM1](images/tim1.png)

#### Настройка DMA

Включить DMA с синхронизацией по событиям PWM для 4 канала:

1. Настройка GPIO. В CubeMX, на вкладке Pinout & Configuration:

   - Настроить выводы PE11, PE12 как Output.
   - Пины порта выбрать как GPIO_Output.
   - Если все пины используются для передачи, можно выбрать All Output.
   - На вкладке System/GPIO выбрать скорость GPIO (например, High speed).

2. Настройка таймера TIM1:

   - Clock Source: Internal Clock
   - Channel 4: PWM generation, No output
   - Prescaler для задания частоты таймера &mdash; 0.
   - Counter Period для управления периодом передачи &mdash; 10-1.
   - Во вкладке DMA Settings для таймера:

3. Настройка DMA:

   - На вкладке DMA: новый DMA2_Stream4.
   - Mode: Memory-to-Peripheral.
   - Priority: High.
   - Peripheral Increment: Disabled.
   - Memory Increment: Enabled.
   - Peripheral Data Width: Word (32 бит).
   - Memory Data Width: Word (32 бит).
   - FIFO mode: Disabled.

![Настройки DMA](images/dma.png)

Ширина данных DMA — WORD, то есть, инкремент данных в памяти на 32 бита — `uint32_t`. Таким образом, можно отправлять данные на весь порт, например, GPIOE. Однако, благодаря использованию регистра BSRR можно отправлять и на определённые пины, такие как PE11 и PE12, не затрагивая остальные.

#### Настройка GPIO

![Настройка GPIO](images/gpio.png)

И не забыть включить прерывание на PC4 (EXTI line[9:5] interrupts).

![Включение прерывания](images/nvic.png)

## Настройка массива данных

Чтобы включить PE11, необходимо установить:

```c
1ULL << 0x10
```

Чтобы выключить PE11, необходимо установить (сдвинуть на 16 бит):

```c
1ULL << (12 + 0x10)
```

Таким образом, получаем функцию инициализации:

```c
static void s_data_init(void)
{
	uint8_t flag = 0;
	for (uint32_t i = 0; i < DATA_SIZE; i++)
	{
		s_data[i] = (1ULL << (11 + 0x10)) | (1ULL << (12 + 0x10));
		if (i % 50 == 0) flag ^= 1;

		if (i >= 100 && i < 600)
		{
			s_data[i] = flag ? (1ULL << (11 + 0x10)) | (1ULL << (12)) : (1ULL << (11)) | (1ULL << (12 + 0x10));
		}
	}
	s_data[DATA_SIZE - 1] = (1ULL << (11 + 0x10)) | (1ULL << (12 + 0x10));
}
```

Начиная с отметки в 10 мкс (сто тактов) каждые 50 тактов (5 мкс) полярности пинов PE11 и PE12 меняются. Начиная с 60 мкс генерация сигнала на пинах PE11 и PE12 прекращается. Соответственно, можно настроить любые пины GPIOE.

Можно сократить количество точек или уменьшить частоту считывания данных DMA. Однако, если нужно будет динамически настраивать частоту семплирования (скажем, от 100 КГц до 1МГц) и с высокой точностью изменять конфигурацию сигнала (задержка, длина импульсной полосы и т.д.), такое решение будет подходящим.

## Инициализация таймеров TIM1, TIM2

Осталось запустить таймер TIM1 для синхронизации отправляемых данных по DMA и TIM2 для генерации тестового сигнала:

```c
// Запуск таймера синхронизации канала DMA для передачи данных
if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK)
{
	Error_Handler();
}

// Таймер 2 выдаёт короткий сигнал, который коммутируется на PC4
// И запускает передачу по DMA
if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
{
	Error_Handler();
}
```

## Функция запуска передачи DMA

Важно обязательно проверять состояние `hdma_tim1_ch4_trig_com`, и если канал занят, сбросить его. Иначе, повторной генерации не произойдет.

Основная функция: `HAL_DMA_Start(&hdma_tim1_ch4_trig_com, (uint32_t)s_data, (uint32_t) &GPIOE->BSRR, DATA_SIZE)`

```c
void s_start_dma_transfer (void)
{
	if (HAL_DMA_GetState(&hdma_tim1_ch4_trig_com) != HAL_DMA_STATE_READY)
	{
		if (HAL_DMA_Abort(&hdma_tim1_ch4_trig_com) != HAL_OK)
		{
			Error_Handler ();
		}
	}

	if (HAL_DMA_Start(&hdma_tim1_ch4_trig_com, (uint32_t)s_data, (uint32_t) &GPIOE->BSRR, DATA_SIZE) != HAL_OK)
	{
		Error_Handler();
	}

	__HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_CC4);

	if (!(htim1.Instance->CR1 & TIM_CR1_CEN))
	{
		__HAL_TIM_ENABLE(&htim1);
	}
}
```

### **Описание макроса `__HAL_TIM_ENABLE_DMA(&htim, TIM_DMA_CC4);`**

Макрос `__HAL_TIM_ENABLE_DMA` включает запрос DMA для указанного события таймера.

#### Формат:

`__HAL_TIM_ENABLE_DMA(&htim, TIM_DMA_CC4);`

#### Параметры:

1. **`&htim`** — указатель на структуру `TIM_HandleTypeDef`, описывающую таймер, который будет связан с DMA.
2. **`TIM_DMA_x`** — определяет событие таймера, которое активирует запрос DMA. Возможные значения:
   * `TIM_DMA_UPDATE` — событие обновления таймера (Update Event).
   * `TIM_DMA_CC1` — событие захвата/сравнения для канала 1 (Capture/Compare Channel 1).
   * `TIM_DMA_CC2` — событие захвата/сравнения для канала 2.
   * `TIM_DMA_CC3` — событие захвата/сравнения для канала 3.
   * `TIM_DMA_CC4` — событие захвата/сравнения для канала 4.
   * `TIM_DMA_TRIGGER` — событие триггера (Trigger Event).
   * `TIM_DMA_COM` — событие управления (Communication Event, для некоторых таймеров).

Этот вызов включает запрос DMA при событии захвата/сравнения (Capture/Compare) на **канале 4 таймера TIM1**.

---

### **Реализация макроса**

Макрос определён в HAL-библиотеке как:

```c
#define __HAL_TIM_ENABLE_DMA(__HANDLE__, __DMA__) ((__HANDLE__)->Instance->DIER |= (__DMA__))
```

* `__HANDLE__` — это указатель на таймер (например, `&htim1`).
* `__DMA__` — битовая маска события DMA.

Фактически, макрос устанавливает соответствующий бит в регистре **`DIER` (DMA/Interrupt Enable Register)** таймера.

### **Регистры таймера**

Регистр **DIER** определяет, какие события таймера активируют запрос DMA. Для примера:

* **`TIM_DMA_CC4`** соответствует биту **CC4DE** в регистре DIER.

```c
TIM1->DIER |= TIM_DMA_CC4;
```

Этот код включает бит **CC4DE** в регистре DIER таймера TIM1, что разрешает запросы DMA для события **Capture/Compare Channel 4**.

## *Описание макроса `__HAL_TIM_ENABLE(&htim);`*

Макрос `__HAL_TIM_ENABLE` используется в библиотеке HAL для включения таймера, то есть для запуска счётчика таймера. Этот макрос записывает в соответствующий регистр таймера значение, которое переводит его из состояния остановки в активное состояние.

```c
__HAL_TIM_ENABLE(&htim);
```

#### Параметры:

* **`&htim`** — указатель на структуру `TIM_HandleTypeDef`, представляющую таймер, который нужно включить.

### **Реализация макроса**

Макрос определён в HAL следующим образом:

```c
#define __HAL_TIM_ENABLE(__HANDLE__) ((__HANDLE__)->Instance->CR1 |= (TIM_CR1_CEN))
```

* `__HANDLE__` — указатель на структуру таймера.
* `Instance` — базовый адрес периферии таймера.
* `CR1` — регистр управления таймером (Control Register 1).
* `TIM_CR1_CEN` — бит, включающий счётчик таймера (Counter Enable).

Когда этот макрос вызывается, устанавливается бит **CEN** в регистре **CR1**. Это включает счётчик таймера, и он начинает работать.

---

### **Функциональность бита CEN**

* **0:** Таймер остановлен. Значение счётчика сохраняется, пока таймер не будет снова включён.
* **1:** Таймер запущен. Счётчик начинает увеличиваться (или уменьшаться, если настроен на режим уменьшения).

### **Когда использовать `__HAL_TIM_ENABLE`?**

* Когда таймер нужно запустить вручную после настройки.
* Когда требуется гибкий контроль над моментом старта таймера (например, после настройки других периферий).

---

### **Взаимодействие с другими функциями**

* Если используется `HAL_TIM_Base_Start` или `HAL_TIM_PWM_Start`, они уже вызывают `__HAL_TIM_ENABLE` внутри себя.
* Если вы работаете напрямую с регистрами, вызов `__HAL_TIM_ENABLE` нужен, чтобы явно включить таймер.

---

### **Отличие от других функций и макросов**

* **`__HAL_TIM_ENABLE`** — просто включает таймер.
* **`HAL_TIM_Base_Start`** — включает таймер и его базовую функциональность, включая прерывания (если настроены).
* **`HAL_TIM_PWM_Start`** — включает таймер и начинает генерацию PWM-сигнала.

---

### **Проверка состояния таймера**

Чтобы проверить, включён ли таймер, можно проверить бит CEN:

```c
if (htim1.Instance->CR1 & TIM_CR1_CEN) {
    // Таймер включён
} else {
    // Таймер выключен
}
```

## Запуск по прерыванию

### Таймер, генератор сигнала

1. TIM2, Channel 1, PWM Generation Channel 1
2. Прескаллер — 500-1, счетчик — 200-1.
3. Счётчик PWM (Pulse): 1.
   Таким образом, на PA0 получится импульс с частотой 2 мс, шириной 5 мкс.

Настройка прерывания на вход на пине PC5 и соединение их проводом.

### Прерывание на пине `PC5`

1. Настроен пин PС5 (9:5) на прерывание по фронту (RISING EDGE).
2. При обнаружении сигнала на пине вызывается `HAL_GPIO_EXTI_Callback`.

#### Обработчик прерывания

В обработчике `HAL_GPIO_EXTI_Callback` ([main.c](Core/Src/main.c)) вызывается функция `Start_DMA_Transfer` ([pulse.c](Core/Src/pulse.c)):

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == INPUT_Pin) {
    Start_DMA_Transfer();
  }
}
```

#### *Функция `Start_DMA_Transfer`*:

Проверяет состояние DMA. Если оно занято, выполняет остановку текущей передачи с помощью `HAL_DMA_Abort()`. Перезапускает передачу данных из массива dma_data в регистр `GPIOE->BSRR`.

## *Выводы*

На осциллограмме хорошо виден результат генерации сигнала. Расстояние между восходящим фронтом импульса пуска и запуском передачи DMA — 4.5 мкс.

1. 1-й канал (<font color="yellow">жёлтый</font>) — входной сигнал от PC5.
2. 2-й канал (<font color="blue">голубой</font>) — выходной сигнал от PE11.
3. 3-й канал (<font color="violet">фиолетовый</font>) — выходной сигнал от PE12.

![Осциллограмма полученного сигнала](images/oscill.png)

1. Номинальная задержка от получения восходящего фронта по пину, до запуска канала **DMA** &mdash; 4.2 микросекунды.
2. Можно генерировать частоты до 2 МГц.
3. Единественная проблема: для сигнала на частоте тактирования DMA 5 МГц, надо массив длиной 96К. Т.е. памяти для таких игрушек надо ~много~. 

Устройство вполне применимо для управления импульсным устройством питания в пульсирующем режиме, где нужна погрешность задержки от управляющего импульса не более &pm; 2.5us. Например, установка плазменного напыления будет управляться подобным устройством. 
