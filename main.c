#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <stdint.h>

QueueHandle_t countQueue;
volatile uint32_t buttonCount=0;

void GPIO_Init(void)
{
    RCC->APB2ENR|=(1U<<2)|(1U<<3)|(1U<<0);

    GPIOB->CRL&=~0xFU;
    GPIOB->CRL|=0x8U;
    GPIOB->ODR|=(1U<<0);

    GPIOA->CRH&=~((0xFU<<4)|(0xFU<<8));
    GPIOA->CRH|=(0xBU<<4)|(0x4U<<8);
}

void USART1_Init(void)
{
    RCC->APB2ENR|=1U<<14;
    USART1->BRR=0x0045U;
    USART1->CR1=(1U<<13)|(1U<<3)|(1U<<2);
}

void USART1_SendChar(char c)
{
    while(!(USART1->SR&(1U<<7)));
    USART1->DR=(uint8_t)c;
}

void USART1_SendString(char *s)
{
    while(*s) USART1_SendChar(*s++);
}

void EXTI0_Init(void)
{
    AFIO->EXTICR[0]&=~0xFU;
    AFIO->EXTICR[0]|=0x1U;

    EXTI->IMR|=1U;
    EXTI->FTSR|=1U;
    EXTI->RTSR&=~1U;
    EXTI->PR=1U;

    NVIC_SetPriority(EXTI0_IRQn,6);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void EXTI0_IRQHandler(void)
{
    BaseType_t higherPriorityTaskWoken=pdFALSE;
    uint32_t count;

    if(EXTI->PR&1U)
    {
        EXTI->PR=1U;
        count=++buttonCount;
        xQueueSendFromISR(countQueue,&count,&higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}

void UART_Task(void *arg)
{
    uint32_t count;
    char msg[40];
    (void)arg;

    USART1_SendString("FreeRTOS EXTI Counter\r\n");

    while(1)
    {
        if(xQueueReceive(countQueue,&count,portMAX_DELAY)==pdPASS)
        {
            int len=snprintf(msg,sizeof(msg),
                "So lan nhan nut: %lu\r\n",(unsigned long)count);

            for(int i=0;i<len;i++)
                USART1_SendChar(msg[i]);
        }
    }
}

int main(void)
{
    GPIO_Init();
    USART1_Init();

    countQueue=xQueueCreate(8,sizeof(uint32_t));
    if(countQueue==NULL) while(1);

    EXTI0_Init();
    xTaskCreate(UART_Task,"UART",256,NULL,2,NULL);

    vTaskStartScheduler();

    while(1);
}