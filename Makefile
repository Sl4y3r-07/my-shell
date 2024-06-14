all:
	sudo apt-get install libreadline-dev
	g++ main.cpp -o my-shell -lreadline

clean:
	rm my-shell