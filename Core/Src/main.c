#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>

typedef struct
{
    uint8_t pin;
    float freq;
    uint8_t duty;
} LED_Config;

typedef struct
{
    uint8_t led;
    int8_t change;
} LED_Command;

LED_Config led[3]={{0,0.1f,50},{1,1.0f,50},{2,10.0f,50}};
QueueHandle_t q;
uint8_t selected=0;

void GPIO_Init(void)
{
    RCC->APB2ENR|=RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPBEN;

    GPIOA->CRL&=~0xFFFU;
    GPIOA->CRL|=0x111U;

    GPIOB->CRL&=~0xFFU;
    GPIOB->CRL|=0x88U;
    GPIOB->ODR|=(1U<<0)|(1U<<1);

    GPIOB->CRH&=~(0xFU<<8);
    GPIOB->CRH|=(0x8U<<8);
    GPIOB->ODR|=(1U<<10);
}

void LED_Task(void *arg)
{
    LED_Config *p=(LED_Config*)arg;
    LED_Command cmd;
    uint8_t id=p->pin;
    TickType_t period=pdMS_TO_TICKS((uint32_t)(500.0f/p->freq));
    TickType_t onTime;

    while(1)
    {
        while(xQueueReceive(q,&cmd,0)==pdPASS)
        {
            if(cmd.led==id)
            {
                if(cmd.change>0)
                {
                    if(p->duty<=75)
                        p->duty+=25;
                    else
                        p->duty=100;
                }
                else if(cmd.change<0)
                {
                    if(p->duty>=25)
                        p->duty-=25;
                    else
                        p->duty=0;
                }
            }
        }

        onTime=(period*p->duty)/100U;

        GPIOA->BSRR=(1U<<p->pin);
        vTaskDelay(onTime);

        GPIOA->BRR=(1U<<p->pin);
        vTaskDelay(period-onTime);
    }
}

void Button_Task(void *arg)
{
    LED_Command cmd;
    (void)arg;

    while(1)
    {
        if(!(GPIOB->IDR&(1U<<0)))
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            if(!(GPIOB->IDR&(1U<<0)))
            {
                selected=(selected+1U)%3;

                while(!(GPIOB->IDR&(1U<<0)))
                    vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        if(!(GPIOB->IDR&(1U<<1)))
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            if(!(GPIOB->IDR&(1U<<1)))
            {
                cmd.led=selected;
                cmd.change=1;
                xQueueSend(q,&cmd,portMAX_DELAY);

                while(!(GPIOB->IDR&(1U<<1)))
                    vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        if(!(GPIOB->IDR&(1U<<10)))
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            if(!(GPIOB->IDR&(1U<<10)))
            {
                cmd.led=selected;
                cmd.change=-1;
                xQueueSend(q,&cmd,portMAX_DELAY);

                while(!(GPIOB->IDR&(1U<<10)))
                    vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

int main(void)
{
    GPIO_Init();

    q=xQueueCreate(8,sizeof(LED_Command));
    if(q==NULL)
        while(1);

    xTaskCreate(LED_Task,"LED0",128,&led[0],1,NULL);
    xTaskCreate(LED_Task,"LED1",128,&led[1],1,NULL);
    xTaskCreate(LED_Task,"LED2",128,&led[2],1,NULL);
    xTaskCreate(Button_Task,"BUTTON",192,NULL,2,NULL);

    vTaskStartScheduler();

    while(1);
}