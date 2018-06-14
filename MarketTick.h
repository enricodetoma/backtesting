#pragma once

#include <vector>
#include <boost/function.hpp>

struct MarketData;
class BaseMarketReplayer;
struct Instrument;

struct MarketTick
{
	enum TickTypes
	{
		DATA,
		TIMER,
		ORDER,
		START_TRADING,
		STOP_TRADING,
		CLOSE_POSITIONS,
	};

	enum ConnectionTypes
	{
		NO_CHANGE,
		CONNECTED,
		DISCONNECTED,
	};

	// E' comodo impostare un puntatore al replayer, in modo che dopo aver consumato un tick dalla priority queue
	// si possa chiamare subito il replayer per ottenere un nuovo tick
	BaseMarketReplayer* replayer;
	enum TickTypes type;
	Instrument* instrument;
	// Nel caso degli start/stop ticks, il timestamp puo' essere in data 1400-Jan-01 quando c'e' solo l'ora senza la data
	// in quel caso bisogna prendere la data corrente da MarketReplayerManager::getCurrentTime()
	double timestamp;
	unsigned long order_if_same_timestamp;
	std::string timestamp_to_be_parsed; // usato dagli start/stop ticks
	std::vector<double> bid_size;
	std::vector<double> bid_price;
	std::vector<double> ask_price;
	std::vector<double> ask_size;
	double last_price;
	double volume;
	double delta_volume;
	double imbalance;
	double auction_price;
	double open_price;
	double close_price;
	bool shortable;
	enum ConnectionTypes connection;
	// Questo serve per il tick di tipo TIMER
	boost::function<void(void)> handler;
	// Questo serve per il tick di tipo ORDER
	long order_id;

	explicit MarketTick(BaseMarketReplayer* replayer_);
	int getBookDepth() const { return book_depth; }
	void setBookDepth(int _book_depth);
	bool readNextTick();

private:
	int book_depth;
};
