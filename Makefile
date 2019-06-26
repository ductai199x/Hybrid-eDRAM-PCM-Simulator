SOURCE  := Main.cc src/Sim/config.cc src/PCMSim/Array_Architecture/pcm_sim_array.cc src/PCMSim/Controller/pcm_sim_controller.cc

CC      := g++
FLAGS   := -O3 -std=c++17 -w -I include
TARGET  := PCMSim

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm $(TARGET)                                                                                   