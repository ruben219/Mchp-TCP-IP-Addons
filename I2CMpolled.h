/*	I2C Master Polled Library
	Copyright (C) 2010 Jesus Ruben Santa Anna Zamudio.
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*This file has been prepared for Doxygen automatic documentation generation.*/

/**
@file I2CMpolled.h
@brief This is the header file for the I2C Master Polled Library.
@ingroup On-Chip Peripherals
@defgroup I2CMpolled I2C Master Polled Library

@code #include "I2CMpolled.h"@endcode
@par Overview
	This is a basic I2C library to control the I2C module commonly found on microcontrollers. This
	library is designed to work on a master device.	Functions are provided to generate most of the 
	conditions required on the bus for data transfer.
	
	This functions are blocking, calling them will cause the processor to stop executing the program
	untill the I2C peripheral finishes the requested operation.

@note <<<<PLACEHOLDER FOR NOTE>>>>

@par Requirements
	<<<<PLACEHOLDER FOR REQUIREMENTS>>>>

@par Usage
	<<<<PLACEHOLDER FOR USAGE INFORMATION>>>>
	@code foo = fooInitialize();@endcode
	<<<<PLACEHOLDER FOR USAGE INFORMATION>>>>
@warning <<<<PLACEHOLDER FOR WARNING MESSAGE>>>>

@author 		Jesus Ruben Santa Anna Zamudio.
@date			<<<<PLACEHOLDER FOR DATE>>>>
@version		<<<<PLACEHOLDER FOR VERSION>>>>
@todo			<<<<PLACEHOLDER FOR TODO INFO>>>>

*/

#ifndef I2CMpolled_H
#define I2CMpolled_H

/*-------------------------------------------------------------*/
/*				Includes and dependencies						*/
/*-------------------------------------------------------------*/

/*=============================================================*/
/*				Configuration and parameters					*/
/*=============================================================*/

/*=============================================================*/
/* End of configuration, do not touch anything below this line 	*/
/*=============================================================*/

/*-------------------------------------------------------------*/
/*				Macros and definitions							*/
/*-------------------------------------------------------------*/

/*-------------------------------------------------------------*/
/*				Typedefs enums & structs						*/
/*-------------------------------------------------------------*/

/*-------------------------------------------------------------*/
/*				Function prototypes								*/
/*-------------------------------------------------------------*/
void I2CStart();
void I2CRestart();
void I2CStop();
void I2CAck();
void I2CNack();
void I2CMIdle();
char I2CRead();
char I2CWrite(unsigned char data_out);
#endif
// End of Header file
