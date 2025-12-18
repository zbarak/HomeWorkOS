SOURCE = src/main.c src/func.c
FLAGS = -Wall -Wextra
TARGET= hw1shell


all: $(SOURCE) 
	@gcc $(FLAGS) $(SOURCE) -o $(TARGET)
	
run: $(SOURCE) 
	@gcc $(FLAGS) $(SOURCE) -o $(TARGET)
	@./$(TARGET)

clean: $(SOURCE) 
	@rm $(TARGET)