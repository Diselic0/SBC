#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
//#include "time_reference.h"

#define Q1  GPIO_NUM_12
#define Q2  GPIO_NUM_14
#define Q3  GPIO_NUM_27
#define Q4  GPIO_NUM_26

#define Q5  GPIO_NUM_17
#define Q6  GPIO_NUM_5
#define Q7  GPIO_NUM_18
#define Q8  GPIO_NUM_19



#define STACK_SIZE  1024*2

static const char* TAG = "stepper";
uint8_t estado_Left[4]; 
uint8_t estado_Right[4]; 
uint8_t nextState_Left;
uint8_t nextState_Right;
uint8_t forOrBack; //forwards = 1
uint16_t ppr_L = 0; //pasos que lleva dados en la rotación.
uint16_t ppr_R = 0;

//for  Quing masage between tasks
/*struct AMessage
{
 char ucMessageID;
 char ucData[ 20 ];
} xMessage;

uint32_t ulVar = 10UL;
*/

esp_err_t init_stepper_controller(void);
esp_err_t toggle_estado_Left(uint8_t forOrBack);
esp_err_t toggle_estado_Right(uint8_t forOrBack);
esp_err_t create_task(void);

void vTask_StepperDrive_Left(void *pvParameters);
void vTask_StepperDrive_Right(void *pvParameters);



void app_main(void)
{

    init_stepper_controller();
    create_task();
    while(1)
    {
        ESP_LOGI(TAG,"free cpu");
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }

}

esp_err_t init_stepper_controller(void)
{
    // config the ESP pins

    gpio_reset_pin(Q1);
    gpio_set_direction(Q1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(Q2);
    gpio_set_direction(Q2, GPIO_MODE_OUTPUT);
    gpio_reset_pin(Q3);
    gpio_set_direction(Q3, GPIO_MODE_OUTPUT);
    gpio_reset_pin(Q4);
    gpio_set_direction(Q4, GPIO_MODE_OUTPUT);

    gpio_reset_pin(Q5);
    gpio_set_direction(Q5, GPIO_MODE_OUTPUT);
    gpio_reset_pin(Q6);
    gpio_set_direction(Q6, GPIO_MODE_OUTPUT);
    gpio_reset_pin(Q7);
    gpio_set_direction(Q7, GPIO_MODE_OUTPUT);
    gpio_reset_pin(Q8);
    gpio_set_direction(Q8, GPIO_MODE_OUTPUT);

    /*We load the first state in the state machine
     State | Q1 | Q2 | Q3 | Q4 |
      1    | 1  | 0  | 1  | 0  |
    */

    for (int i = 0; i < 4; i++)
    {
        if (i % 2 == 0)
        {
            estado_Left[i] = 0;
            estado_Right[i] = 0;
        }
        else
        {
            estado_Left[i] = 1;
            estado_Right[i] = 0;
        }
    }
    nextState_Left = 1;
    nextState_Right = 1;

    forOrBack = 1;

    return ESP_OK;
}

esp_err_t toggle_estado_Right(uint8_t forOrBack)
{
     /*States machine :
     State | Q1 | Q2 | Q3 | Q4 |
      1    | 1  | 0  | 1  | 0  |
      2    | 1  | 0  | 0  | 1  |
      3    | 0  | 1  | 0  | 1  |
      4    | 0  | 1  | 1  | 0  |
    */

    if (forOrBack)
    {
        nextState_Right++;
        if (nextState_Right > 4)
        nextState_Right = 1;
    }

    else 
    {
        nextState_Right--;
        if (nextState_Right < 1)
        nextState_Right = 4;

    }

    
    
    switch (nextState_Right)
    {
    case 1:
        estado_Right[0] = 1;
        estado_Right[1] = 0;
        break;

    case 2:
        estado_Right[2] = 0;
        estado_Right[3] = 1;
        break;

    case 3:
        estado_Right[0] = 0;
        estado_Right[1] = 1;
        break;

    case 4:
        estado_Right[2] = 1;
        estado_Right[3] = 0;
        break;

        // should never go there. If for some reason next state bugs it resets to its firs state.
    default:
        for (int i = 0; i < 4; i++)
        {
            if (i % 2 == 0)
                estado_Right[i] = 0;
            else
                estado_Right[i] = 1;
        }
        nextState_Right= 1;
        break;
    }

    
   ESP_LOGI (TAG, "SIGUIENTE ESTADO : %d \n", nextState_Right);
    return ESP_OK;

}
esp_err_t toggle_estado_Left(uint8_t forOrBack)

{
    /*States machine :
     State | Q1 | Q2 | Q3 | Q4 |
      1    | 1  | 0  | 1  | 0  |
      2    | 1  | 0  | 0  | 1  |
      3    | 0  | 1  | 0  | 1  |
      4    | 0  | 1  | 1  | 0  |
    */

     if (forOrBack)
    {
        nextState_Left++;
        if (nextState_Left > 4)
        nextState_Left = 1;
    }

    else 
    {
        nextState_Left--;
        if (nextState_Left < 1)
        nextState_Left = 4;

    }


    switch (nextState_Left)
    {
    case 1:
        estado_Left[0] = 1;
        estado_Left[1] = 0;
        break;

    case 2:
        estado_Left[2] = 0;
        estado_Left[3] = 1;
        break;

    case 3:
        estado_Left[0] = 0;
        estado_Left[1] = 1;
        break;

    case 4:
        estado_Left[2] = 1;
        estado_Left[3] = 0;
        break;

        // should never go there. If for some reason next state bugs it resets to its firs state.
    default:
        for (int i = 0; i < 4; i++)
        {
            if (i % 2 == 0)
                estado_Left[i] = 0;
            else
                estado_Left[i] = 1;
        }
        nextState_Left= 1;
        break;
    }

    return ESP_OK;
}

esp_err_t create_task(void)
{
    static uint8_t ucParameterToPass_L;
    static uint8_t ucParameterToPass_R;
    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(vTask_StepperDrive_Left,     //Pointer to the task entry function.
                            "vTask_StepperDrive_Left",   //A descriptive name for the task.               
                            STACK_SIZE,             //the size of the task stack specified as the number of bytes.
                            &ucParameterToPass_L,    
                            tskIDLE_PRIORITY,       //The priority at which the task should run.
                            &xHandle,               //Use the handle to delete the task.
                            0);
    
     xTaskCreatePinnedToCore(vTask_StepperDrive_Right,     //Pointer to the task entry function.
                            "vTask_StepperDrive_Right",   //A descriptive name for the task.               
                            STACK_SIZE,             //the size of the task stack specified as the number of bytes.
                            &ucParameterToPass_R,    
                            tskIDLE_PRIORITY,       //The priority at which the task should run.
                            &xHandle,               //Use the handle to delete the task.
                            0);

    configASSERT(xHandle);
    return ESP_OK;
}

void vTask_StepperDrive_Left(void *pvParameters)
{
    for (;;)
    {
        //goes to the next state
        toggle_estado_Left(1);
        ppr_L++;
        gpio_set_level(Q1, estado_Left[0]);
        gpio_set_level(Q2, estado_Left[1]);
        gpio_set_level(Q3, estado_Left[2]);
        gpio_set_level(Q4, estado_Left[3]);
        ESP_LOGW(TAG, "Paso: %d , pasos abanzados en la rotación: %d, angulo recorrido: %f \n", nextState_Left, ppr_L, ppr_L*7.5);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void vTask_StepperDrive_Right(void *pvParameters)
{
    for (;;)
    {
        //goes to the next state
        toggle_estado_Right(0);
        ppr_R++;
        gpio_set_level(Q5, estado_Right[0]);
        gpio_set_level(Q6, estado_Right[1]);
        gpio_set_level(Q7, estado_Right[2]);
        gpio_set_level(Q8, estado_Right[3]);
        ESP_LOGW(TAG, "Paso: %d , pasos abanzados en la rotación: %d, angulo recorrido: %f \n", nextState_Right, ppr_R, ppr_R*7.5);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
/*
QueueHandle_t xQueue;

// Function to create a queue and post some values.
void vAFunction( void *pvParameters )
{j 
char cValueToPost;
const TickType_t xTicksToWait = ( TickType_t )0xff;

 // Create a queue capable of containing 10 characters.
 xQueue = xQueueCreate( 10, sizeof( char ) );
 if( xQueue == 0 )
 {
     // Failed to create the queue.
 }

 // ...

 // Post some characters that will be used within an ISR.  If the queue
 // is full then this task will block for xTicksToWait ticks.
 cValueToPost = 'a';
 xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
 cValueToPost = 'b';
 xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );

 // ... keep posting characters ... this task may block when the queue
 // becomes full.

 cValueToPost = 'c';
 xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
}

// ISR that outputs all the characters received on the queue.
void vISR_Routine( void )
{
BaseType_t xTaskWokenByReceive = pdFALSE;
char cRxedChar;

 while( xQueueReceiveFromISR( xQueue, ( void * ) &cRxedChar, &xTaskWokenByReceive) )
 {
     // A character was received.  Output the character now.
     vOutputCharacter( cRxedChar );

     // If removing the character from the queue woke the task that was
     // posting onto the queue cTaskWokenByReceive will have been set to
     // pdTRUE.  No matter how many times this loop iterates only one
     // task will be woken.
 }

 if( cTaskWokenByPost != ( char ) pdFALSE;
 {
     taskYIELD ();
 }
}*/