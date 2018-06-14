# backtesting in C++

See https://quant.stackexchange.com/questions/39995/backtesting-c-algorithms/40023#40023

`MarketTick` is the data structure which is produced by classes which inherit from `BaseMarketReplayer`.
Each of these derived classes represent a reader for a specific data format (e.g. from binary or SQLite or text files) and must implement `bool readNextTick(MarketTick& tick)` to produce a new tick of data when requested.

`MarketReplayerManager` implements the whole logic of populating the `std::priority_queue` of `MarketTick`s by calling `readNextTick()` every time a tick from a replayer is consumed.

Below is the part of the code which simulates orders with a delay by inserting a `MarketTick::ORDER` into the `std::priority_queue`:
```
if (_trader_manager->_market_replay)
{
	// In market replay, l'ordine viene ritardato di _order_delay_replay_seconds secondi
	// per simulare un ritardo da parte del broker
	if (_trader_manager->getReplayerManager(TraderManager::MarketReplayerManagerKey()))
	{
		std::shared_ptr<MarketTick> tick(new MarketTick(NULL));
		tick->type = MarketTick::ORDER;
		tick->timestamp = _trader_manager->getReplayerManager(TraderManager::MarketReplayerManagerKey())->getCurrentTime() +
			_trader_manager->_order_delay_replay_seconds * 1000;
		// Se l'ordine e' marketable, allora lo inseriamo come immediate or cancel e assumiamo che sia interamente eseguito
		OpenOrder open_order(instrument, order_action, quantity, order_type, limit_price, dynamics,
		                     instrument->market_data->getExecutableBidPrice(),
		                     instrument->market_data->getExecutableAskPrice());
		BOOST_LOG_TRIVIAL(info) << (open_order.action == LuaEnums::BUY ? "BUY " : "SELL ") << open_order.quantity << " " <<
			open_order.instrument->name << " @ " << (open_order.type == LuaEnums::MARKET ? "MARKET" : "") << (
				open_order.type == LuaEnums::MARKET ? 0.0 : open_order.limit_price);
		// In simulazione devo dare anche il broker_id
		open_order.id = open_order.broker_id = _next_order_id++;
		auto pair = _pending_orders.insert(open_order);
		if (!pair.second)
		{
			// Qui non dovrebbe mai capitare
			BOOST_LOG_TRIVIAL(error) << "Unexpected error: duplicated broker id " << open_order.broker_id << " for instrument "
				<< instrument->name;
			return 0;
		}
		pair.first->checkAssumeFullExecution();
		tick->order_id = open_order.id;
		_trader_manager->getReplayerManager(TraderManager::MarketReplayerManagerKey())->addNewTick(tick);
		return open_order.id;
	}
	else
	{
		BOOST_LOG_TRIVIAL(error) << "Missing market replayer manager";
		return 0;
	}
}
```
