#make library
KLIB_DIR = $(DESTDIR)/usr/local/lib
CFLAGS += -D__linux__ -Dplatform_64
 
INC_DIR = -I../include
	  
APP_FLAGS = -DLinux \
		-Dplatform_64 \
		$(INC_DIR) \
								 
libnacapi.a: ../include/nacapi.h
	$(CC) -c nacapi.c $(APP_FLAGS) -o nacapi.o
	$(CC) -c nac_stream.c $(APP_FLAGS) -o nac_stream.o
	$(AR) rcs libnacapi.a nacapi.o nac_stream.o
	ranlib libnacapi.a

clean:
	rm -f *.o *.a
