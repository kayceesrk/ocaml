all: libvergc.a

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

libvergc.a: FStar.o LowStar_Prims.o Spec.o alloc.o gc.o librust_allocator.a
	$(AR) rcs $@ $^

librust_allocator.a:
	$(MAKE) -C allocator
	cp allocator/target/release/librust_allocator.a .

clean:
	$(MAKE) -C allocator clean
	rm -f *.o *.a *~
