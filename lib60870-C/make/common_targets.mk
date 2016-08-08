$(LIB_NAME):
		cd $(LIB60870_HOME); $(MAKE) -f Makefile

lib:	$(LIB_NAME)
	
libclean:	clean
	cd $(LIB60870_HOME); $(MAKE) -f Makefile clean
