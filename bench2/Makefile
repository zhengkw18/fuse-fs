PROG=bench2
OBJDIR=.obj
SRCDIR=../src
CC=g++

CFLAGS = -Wall --std=c++14 `pkg-config fuse3 --cflags` -I..
LDFLAGS = `pkg-config fuse3 --libs`

$(shell mkdir -p $(OBJDIR)) 

OBJS = $(OBJDIR)/bench2.o $(OBJDIR)/easyfs.o $(OBJDIR)/bitmap.o $(OBJDIR)/block_cache.o $(OBJDIR)/block_dev.o $(OBJDIR)/efs.o $(OBJDIR)/layout.o $(OBJDIR)/vfs.o

$(PROG) : $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(PROG)

-include $(OBJS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) -c $(CFLAGS) $(SRCDIR)/$*.cpp -o $(OBJDIR)/$*.o
	$(CC) -MM $(CFLAGS) $(SRCDIR)/$*.cpp > $(OBJDIR)/$*.d
	@mv -f $(OBJDIR)/$*.d $(OBJDIR)/$*.d.tmp
	@sed -e 's|.*:|$(OBJDIR)/$*.o:|' < $(OBJDIR)/$*.d.tmp > $(OBJDIR)/$*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $(OBJDIR)/$*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $(OBJDIR)/$*.d
	@rm -f $(OBJDIR)/$*.d.tmp

clean:
	rm -rf $(PROG) $(OBJDIR)

