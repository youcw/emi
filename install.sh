#!/usr/bin/python2
import os

confile=open('config.mk','w')

KEY=raw_input('do you mean to cross-compile?,(default is no)')
if KEY == 'YES' or KEY == 'yes' or KEY == 'Y' or KEY == 'y':
	CROSS=raw_input('what is your compile suffix?')
	confile.write('CROSS=')
	confile.write(CROSS)
	confile.write('\n')
else:
	confile.write('CROSS=\n')

KEY=raw_input('what is your arch?,(default is x86)')
if KEY == '':
	confile.write('ARCH=x86\n')
else:
	confile.write('ARCH=')
	confile.write(KEY)
	confile.write('\n')

KEY=raw_input('where do you want to install?,(default is /usr/local)')
if KEY == '':
	confile.write('PREFIX=/usr/local\n')
else:
	confile.write('PREFIX=')
	confile.write(KEY)
	confile.write('\n')

KEY=raw_input('do you want to debug?,(default is yes)')
if KEY == 'NO' or KEY == 'no' or KEY == 'N' or KEY == 'n':
	confile.write('DEBUG=\n')
else:
	confile.write('DEBUG=-g -DDEBUG\n')

KEY=raw_input('do you build static emi_core?,(default is yes)')
if KEY == 'NO' or KEY == 'no' or KEY == 'N' or KEY == 'n':
	confile.write('STATIC=\n')
else:
	confile.write('STATIC=-static');

os.system("cp Makefile.am Makefile")
