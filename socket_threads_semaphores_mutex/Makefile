server:
	@echo "Building server..."
	@gcc server.c -lgsl -lgslcblas -lm -g -o PideShop 
	@./PideShop 127.0.0.1 8080 5 10 5
client:
	@echo "Building client..."
	@gcc client.c -o HungryVeryMuch
	@./HungryVeryMuch 127.0.0.1 8080 3 10 20

clean:
	@echo "Cleaning up..."
	@rm -f server client
	@echo "Done."
	@rm -f PideShop HungryVeryMuch
	@rm -f *.o *.out 
	@rm -f server.log client.log 