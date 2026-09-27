// Stephen Kanti Mahanty
// Made 9/27/26
// Plays my own song

// // Pitch in Hz, duration in ms
//


#include <stdint.h>

#define GPIOA_BASE     (0x48000000UL)

typedef struct {
    volatile uint32_t MODER;   // 0x00
    volatile uint32_t OTYPER;  // 0x04
    volatile uint32_t OSPEEDR; // 0x08
    volatile uint32_t PUPDR;   // 0x0C
    volatile uint32_t IDR;     // 0x10
    volatile uint32_t ODR;     // 0x14
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *) GPIOA_BASE)

// Definitions for the timer
#define TIM6_BASE (0x40001000UL)
// TIMx control register 1 at offset 0x00

// Typedef for TIM6
typedef struct {
    volatile uint32_t CR1;          // 0x00 - register 1
    volatile uint32_t CR2;          // 0x04
    volatile uint32_t RESERVED0;    // Reserved: 0x08
    volatile uint32_t DIER;         // 0x0C
    volatile uint32_t SR;           // 0x10
    volatile uint32_t EGR;          // 0x14
    volatile uint32_t RESERVED1[3]; // Reserved: 0x18, 0x1C, 0x20
    volatile uint32_t CNT;          // 0x24 - counter
    volatile uint32_t PSC;          // 0x28
    volatile uint32_t ARR;          // 0x2C
} TIM_TypeDef;

#define TIM6 ((TIM_TypeDef *) TIM6_BASE)

// Finding the clock
#define RCC_BASE       (0x40021000UL)
#define RCC_APB1ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x58))
#define RCC_AHB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x4C))

// Timer definitions so I don't have to do -> a bunch
#define TIM_CR1_CEN (1U << 0)
#define TIM_EGR_UG  (1U << 0)
#define TIM_SR_UIF  (1U << 0)

// Turning on clock: RCC_APB1ENR1 |= (1U << 4)
// Turning on counter: TIM6->CR1 |= (1U << 0) (turn off is &= ~(1U << 0))
    // While PSG is on, counter should be cleared through UG bit of TIMx_EGR
    // Counter blocked when ARR is zero

/*
The timer clock frequencies are automatically defined by hardware. There are two cases:
1. If the APB prescaler equals 1, the timer clock frequencies are set to the same
frequency as that of the APB domain.
2. Otherwise, they are set to twice (×2) the frequency of the APB domain.
*/

// Enables TIM6 clock and makes sure CEN is cleared
void initTIM6(void) {
    RCC_APB1ENR1 |= (1U << 4);
    TIM6->CR1 &= ~TIM_CR1_CEN;  // Clear CEN
}

// Stops TIM6 and writes to prescaler and autoreload
void configureTIM6(uint16_t prescaler, uint16_t autoreload) {
    // Stops TIM6
    TIM6->CR1 &= ~TIM_CR1_CEN; 

    // Writes to prescaler
    TIM6->PSC = prescaler;

    // Writes ARR
    TIM6->ARR = autoreload;

    // Has UG load new values
    TIM6->EGR = TIM_EGR_UG;

    // Clears UIF
    TIM6->SR &= ~TIM_SR_UIF;

    // Starts TIM6
    TIM6->CR1 |= TIM_CR1_CEN;

}

// Inspect SR until update flag is 1, then clears UIF
void waitForTIM6Update(void) {
    while ((TIM6->SR & TIM_SR_UIF) == 0U) {
        // Stay here until TIM6 sets UIF
    }

    // Clears UIF
    TIM6->SR &= ~TIM_SR_UIF;
}

/*
PSC to give a 1 MHz counter = 4MHz/1MHz - 1 = 3
ARR to update every 1 ms is (79+1)(ARR + 1)/(80 MHz) = 0.001 -> ARR = 999
*/

// Plays a note
void playnote(uint16_t pitch, uint16_t duration) {
    /* 
    Frequency is pitch so N (number of timer counts per half period) is
        N = 1000000+pitch/2*pitch
    */
    int count;

    if(duration == 0) {
        return;
    }

    // Handle a rest separately to avoid dividing by zero
    if (pitch == 0U) {
        // One timer update per millisecond
        configureTIM6(3, 999);

        for (count = 0; count < duration; count++) {
            waitForTIM6Update();
        }
    } else {
        // Timer counts per half period
        // Dividing by 2 makes sure it rounds to nearest integer
        int n = (1000000 + pitch) / (2*pitch);
        int arr = n - 1;
        count = 0;

        // Number of updates to fit in the duration (s)
        int update = (1000*duration + n / 2)/n;

        configureTIM6(3, arr);

        // Turns off PA9
        GPIOA->ODR &= ~(1U << 9);

        while(count < update) {
            waitForTIM6Update();
            count++;

            GPIOA->ODR ^= (1U << 9);
        }
    }

    // Turns off PA9
    TIM6->CR1 &= ~TIM_CR1_CEN;
    GPIOA->ODR &= ~(1U << 9);
    
}

int main(void) {

    // Enable the GPIOA peripheral clock.
    RCC_AHB2ENR |= (1U << 0);

    // Clear PA9's bits 19:18 and PA12's bits 25:24.
    GPIOA->MODER &= ~((3U << 18) | (3U << 24));

    // Set both pins to 01 (output)
    GPIOA->MODER |=  ((1U << 18) | (1U << 24));

    // Initially drive both pins low.
    GPIOA->ODR &= ~((1U << 9) | (1U << 12));

    initTIM6();

    const int notes[109][2] = {
    // Measure 1: piano
    {220, 235}, // A3
    {  0, 235}, // eighth rest
    {165, 235}, // E3
    {165, 235}, // E3
    {247, 470}, // B3
    {220, 470}, // A3

    // Measure 2: guitar lead
    {  0, 235}, // eighth rest
    {494, 235}, // B4
    {659, 235}, // E5
    {622, 235}, // D#5
    {494, 470}, // B4

    // A4 tied across measures 2 and 3
    {440, 940}, // A4

    // Measure 3
    {  0, 940}, // two quarter rests
    {494, 118}, // B4
    {554, 118}, // C#5
    {587, 235}, // D5

    // Measure 4: guitar lead
    {  0, 235}, // eighth rest
    {587, 235}, // D5
    {740, 235}, // F#5
    {622, 235}, // D#5
    {494, 470}, // B4
    {440, 235}, // A4
    {415, 235}, // G#4

    // Measure 5: piano
    {370, 235}, // F#4
    {370, 235}, // F#4
    {165, 235}, // E3
    {165, 235}, // E3
    {185, 470}, // F#3
    {165, 470}, // E3

    // Beginning of measure 6
    {294, 118}, // D4
    {370, 118}, // F#4
    {  0, 235}, // eighth rest
    {294, 118}, // D4
    {370, 118}, // F#4

    {0, 0}      // End marker
    };

    // Playing the notes
    for(int i = 0; notes[i][0] != 0; i++) {
        playnote(notes[i][0], notes[i][1]);
    }

    while (1) {
    // Song finished
}
}