#include "ch32v003fun.h"
#include "ch32v003_touch.h"
#include <stdio.h>
#include "business_card.h"

void neopixel_bitSend(uint32_t color);
void pushPixel(uint32_t color);
void show(void);
void setAnimation(uint8_t section, uint8_t color, uint8_t animation);
uint32_t rainbow(uint8_t offset);
uint32_t udiv32(uint32_t n, uint32_t d);
uint8_t udiv8(uint8_t n, uint8_t d);
uint8_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);

uint32_t strip[NUM_LEDS];
uint8_t frame = 0;
uint8_t slowFrame = 0;
uint8_t pulseFrame = 0;
uint8_t slowFlop = 0;
uint8_t flag_noSnake = 0;

// Underline is 0-8
// L is 9-15
int main()
{
	SystemInit();

	// Enable GPIOD, C and ADC
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1;

	// GPIO PC1 Push-Pull
	GPIOC->CFGLR &= ~(0xf<<(4*1));
	GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP)<<(4*1);

	// init systick @ 1ms rate
	printf("initializing adc...");
	InitTouchADC();
	printf("done.\n\r");

	uint32_t touch_anim_base = ReadTouchPinSafe(GPIOC, 4, 2, 3);
	uint32_t touch_anim_new = touch_anim_base;
	uint32_t touch_color_base = ReadTouchPinSafe(GPIOD, 6, 6, 3);
	uint32_t touch_color_new = touch_color_base;
	uint8_t flag_anim_touch = 0;
	uint8_t flag_color_touch = 0;
	uint8_t selAnim = 0;
	uint8_t selColor = 0;

	while(1)
	{
		touch_anim_new = ReadTouchPinSafe(GPIOC, 4, 2, 3);
		touch_color_new = ReadTouchPinSafe(GPIOD, 6, 6, 3);
		if(touch_anim_new > touch_anim_base + 100 && !flag_anim_touch){
			flag_anim_touch = 1;
			frame = 0;
			slowFrame = 0;
			selAnim++;
			if(selAnim > 3) selAnim = 0;
		}
		if(touch_color_new > touch_color_base + 100 && !flag_color_touch){
			flag_color_touch = 1;
			frame = 0;
			slowFrame = 0;
			selColor++;
			if(selColor > 8) selColor = 0;
		}
		touch_anim_base = touch_anim_new;
		touch_color_base = touch_color_new;

		setAnimation(0,selColor, selAnim);
		setAnimation(1,selColor, 2);
		show();
		Delay_Ms(10);
		frame++;
		if(!(frame%16)){
			flag_anim_touch = 0; // Debounce reset every 16 cycles
			flag_color_touch = 0;
		}
		if(!(frame%4)){
			slowFrame++;
			if(slowFrame >= 32){
				slowFrame = 0;
				//flag_noSnake = 0;
			}
		}
		if(!(frame%2)){
			pulseFrame++;
			if(pulseFrame >= 64){
				pulseFrame = 0;
			}
		}
		if(!frame){
			slowFlop = !slowFlop;
		}
	}
}

// User function, sends current pixel vals to LED strip
void show(){
	for(int i=0; i<NUM_LEDS; i++){
		pushPixel(strip[i]);
	}
}

// User function, writes desired color to pixel position in pixel array
void writePixel(uint8_t pixel, uint32_t color){
	if(pixel < NUM_LEDS) strip[pixel] = color;
}

/*
 * Evaluate each bit of the color in the correct order.
 * For a standard neopixel, sending the color var 1 bit
 * at a time starting from the MSB will yield the
 * correct results.
 */
void pushPixel(uint32_t color){	
	for(uint8_t i=0; i<24; i++){
		neopixel_bitSend(color);
		color <<= 1;
	}
}

// Bit banging to match neopixel high/low timings
void neopixel_bitSend(uint32_t color){
	if(color & 0x800000){ // Write PC1, 1 = HIGH
		GPIOC->BSHR = 1<<(1);
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		GPIOC->BSHR = (1<<(16+1));
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop");
	}
	else{ // Write PC1, 0 = LOW
		GPIOC->BSHR = 1<<(1);
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		//asm volatile("nop\nnop\nnop\nnop\nnop");
		GPIOC->BSHR = (1<<(16+1));
		//asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
		asm volatile("nop\nnop\nnop\nnop\nnop\nnop");
	}
}

void setAnimation(uint8_t section, uint8_t color, uint8_t animation){
	uint32_t colorSel;
	uint8_t led_max = section ? NUM_LEDS : LED_SPLIT;
	uint8_t led_min = section ? LED_SPLIT : 0;
	uint8_t step = (uint8_t)udiv32(255, led_max-led_min);
	uint8_t offset = 0;
	for(uint8_t i=led_min; i<led_max; i++){
		switch(animation){
			case 0: //static
				writePixel(i, Color(
					colorList[color][1] >> 4,
					colorList[color][0] >> 4,
					colorList[color][2] >> 4
				));
				break;
			case 1: // pulse
				colorSel = Color(
					map(colorList[color][1], 0, 255, 0, fastSine[pulseFrame]),
					map(colorList[color][0], 0, 255, 0, fastSine[pulseFrame]),
					map(colorList[color][2], 0, 255, 0, fastSine[pulseFrame])
				);
				writePixel(i, colorSel);
				break;
			case 2: // rainbow
				writePixel(i, rainbow(offset));
				break;
			case 3: // snake
				// Light up each pixel one at a time
				// Each pixel dims over time to eventually shut off
				if(i == slowFrame) colorSel = Color(255,0,0);
				if(slowFrame >= led_max + 6) colorSel = 0;
				else if(i < slowFrame){
					colorSel = Color(
						map(colorList[color][1], 0, 255, 0, colorList[color][1] >> (slowFrame - i)),
						map(colorList[color][0], 0, 255, 0, colorList[color][0] >> (slowFrame - i)),
						map(colorList[color][2], 0, 255, 0, colorList[color][2] >> (slowFrame - i))
					);
				}
				else colorSel = 0;
				if(colorSel & 0x00C000) colorSel = 0;
				writePixel(i, colorSel);
				break;
			default:
				break;
		}
		offset += step;
	}
}

uint32_t rainbow(uint8_t offset){
	uint32_t base = color_wheel(frame + offset);
	uint8_t r = RED_VAL(base)>> 4;
	uint8_t g = GREEN_VAL(base)>> 4;
	uint8_t b = BLUE_VAL(base) >> 4;
	return Color(r,g,b);
}

uint32_t udiv32(uint32_t n, uint32_t d) {
    // n is dividend, d is divisor
    // store the result in q: q = n / d
    uint32_t q = 0;

    // as long as the divisor fits into the remainder there is something to do
    while (n >= d) {
        uint32_t i = 0, d_t = d;
        // determine to which power of two the divisor still fits the dividend
        //
        // i.e.: we intend to subtract the divisor multiplied by powers of two
        // which in turn gives us a one in the binary representation 
        // of the result
        while (n >= (d_t << 1) && ++i)
            d_t <<= 1;
        // set the corresponding bit in the result
        q |= 1 << i;
        // subtract the multiple of the divisor to be left with the remainder
        n -= d_t;
        // repeat until the divisor does not fit into the remainder anymore
    }
    return q;
}
uint8_t udiv8(uint8_t n, uint8_t d) {
    // n is dividend, d is divisor
    // store the result in q: q = n / d
    uint8_t q = 0;

    // as long as the divisor fits into the remainder there is something to do
    while (n >= d) {
        uint8_t i = 0, d_t = d;
        // determine to which power of two the divisor still fits the dividend
        //
        // i.e.: we intend to subtract the divisor multiplied by powers of two
        // which in turn gives us a one in the binary representation 
        // of the result
        while (n >= (d_t << 1) && ++i)
            d_t <<= 1;
        // set the corresponding bit in the result
        q |= 1 << i;
        // subtract the multiple of the divisor to be left with the remainder
        n -= d_t;
        // repeat until the divisor does not fit into the remainder anymore
    }
    return q;
}

uint8_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max){
	if(!x) return 0;
	uint32_t mapped = udiv32((x - in_min) * (out_max - out_min), (in_max - in_min) + out_min);
	return (uint8_t)mapped;
}