#include "MarketReplayerManager.h"
#include "TraderManager.h"
#include "OrderManager.h"
#include "IndicatorManager.h"
#include "StrategyManager.h"
#include "PositionManager.h"
#include "IBBidAskMarketReplayer.h"
//#include "SqliteMarketReplayer.h"
#include "BinaryMarketReplayer.h"
#include "TimerMarketReplayer.h"
#include "HelperFunctions.h"
#include "logger.h"
#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <unicode/smpdtfmt.h>
using namespace U_ICU_NAMESPACE;

MarketReplayerManager::MarketReplayerManager(TraderManager* trader_manager, const std::string& default_replayer,
                                             const std::string& pnl_file_name)
	: _tick_counter(0)
	  , _current_time(0.0)
	  , _trader_manager(trader_manager)
	  , _default_replayer(default_replayer)
	  , _pnl_file_name(pnl_file_name)
{
	_combos_to_calculate_market_data_for.clear();
	boost::ptr_vector<Instrument>& instruments = _trader_manager->getInstruments(
		TraderManager::InstrumentsMarketDataPositionsKey());
	for (size_t i = 0; i < instruments.size(); i++)
	{
		BaseMarketReplayer* mkt_repl = nullptr;
		if (instruments[i].simulated_combo)
			_combos_to_calculate_market_data_for.push_back(i);
		else
		{
			std::string replayer = instruments[i].replayer;
			if (replayer.empty())
				replayer = _default_replayer;
			mkt_repl = createMarketReplayer(replayer, &instruments[i]);
			if (mkt_repl && !mkt_repl->error.empty())
			{
				BOOST_LOG_TRIVIAL(error) << mkt_repl->error;
				delete mkt_repl;
				mkt_repl = nullptr;
			}
		}
		_replayers.push_back(mkt_repl);
	}
}

MarketReplayerManager::~MarketReplayerManager(void)
{
}

void MarketReplayerManager::replay()
{
	double gui_time_base;
	double prev_unr_pnl = 0., prev_rea_pnl = 0.;
	_local_start_time = gui_time_base = Calendar::getNow();
	// Pulisce il file degli eseguiti
	_trader_manager->_order_manager->deleteExeFile();
	// Pulisce i file di debug
	_trader_manager->_indicator_manager->deleteAllDebugFiles();
	_trader_manager->_strategy_manager->deleteAllDebugFiles();
	// Apre il file di p&l
	std::ofstream pnl_file(_pnl_file_name.c_str());
	if (pnl_file)
		pnl_file << "Time;Unrealized p&l;Realized p&l;Total p&l\n";
	// Azzera tutti i volumi prima di iniziare il replay
	std::vector<MarketData>& market_data = _trader_manager->getMarketData(
		TraderManager::InstrumentsMarketDataPositionsKey());
	for (size_t i = 0; i < market_data.size(); i++)
		market_data[i].setVolume(0.);
	// Effettua un primo ciclo resettando tutti i replayer e caricando un tick da ciascuno di essi nella priority queue
	int data_replayers_count = 0;
	_market_tick_queue = std::priority_queue<std::shared_ptr<MarketTick>, std::vector<std::shared_ptr<MarketTick>>, PriorityCompareMarketTick>();
	for (boost::ptr_vector<boost::nullable<BaseMarketReplayer>>::iterator it = _replayers.begin(); it != _replayers.end();
	     ++it)
	{
		if (is_null(it))
			continue;
		it->reset();
		std::shared_ptr<MarketTick> tick(new MarketTick(&*it));
		if (tick->readNextTick())
		{
			addNewTick(tick);
			// Conta i data replayer perche' il ciclo successivo terminera' quando tutti i data replayer hanno esaurito i loro dati
			data_replayers_count++;
		}
	}
	if (!_market_tick_queue.empty())
	{
		// Determina l'ora corrente come 100 ms dopo il dato con il timestamp piu' vecchio
		_replay_start_time = _current_time = _market_tick_queue.top()->timestamp + 100;
		// Estraggo i tick di start/stop e li inserisco nella coda, aggiungendo eventualmente la data se e' presente solo l'ora
		for (auto it = _trader_manager->getStartStopTicks(TraderManager::MarketReplayerManagerKey()).begin();
			it != _trader_manager->getStartStopTicks(TraderManager::MarketReplayerManagerKey()).end();
			++it)
		{
			// Parserizza il timestamp
			// Prima prova con i formati con data/ora
			UErrorCode success = U_ZERO_ERROR;
			(*it)->timestamp = _trader_manager->_ymd_hms_formatter->parse(UnicodeString((*it)->timestamp_to_be_parsed.c_str(), -1, US_INV), success);
			if (!U_SUCCESS(success))
			{
				success = U_ZERO_ERROR;
				(*it)->timestamp = _trader_manager->_ymd_hm_formatter->parse(UnicodeString((*it)->timestamp_to_be_parsed.c_str(), -1, US_INV), success);
				if (!U_SUCCESS(success))
				{
					// Se non hanno funzionato direttamente i formati con data/ora, prova ad aggiungere la data corrente
					std::string timestamp_to_be_parsed = _trader_manager->getLocalDate() + " " + (*it)->timestamp_to_be_parsed;
					success = U_ZERO_ERROR;
					(*it)->timestamp = _trader_manager->_ymd_hms_formatter->parse(
						UnicodeString(timestamp_to_be_parsed.c_str(), -1, US_INV), success);
					if (!U_SUCCESS(success))
					{
						success = U_ZERO_ERROR;
						(*it)->timestamp = _trader_manager->_ymd_hm_formatter->parse(
							UnicodeString(timestamp_to_be_parsed.c_str(), -1, US_INV), success);
						if (!U_SUCCESS(success))
						{
							BOOST_LOG_TRIVIAL(error) << "Invalid start/stop tick: " << (*it)->timestamp_to_be_parsed;
							continue;
						}
					}
				}
			}

			addNewTick(*it);
		}
		// Calcola il displacement da UTC (non tiene conto di ora legale/solare perche' non usa la data nel calcolo,
		// ma non ha particolarmente importanza perche' serve solo per capire, indicativamente, quando wrappa
		// il giorno per azzerare i volumi)
		double utc_displacement;
		{
			UErrorCode success = U_ZERO_ERROR;
			const std::unique_ptr<DateFormat> tmp_fromatter(DateFormat::createTimeInstance(DateFormat::SHORT));
			if (_trader_manager->_calendar)
				tmp_fromatter->adoptCalendar(_trader_manager->_calendar->clone());
			utc_displacement = tmp_fromatter->parse(UnicodeString("00:00", -1, US_INV), success);
			if (!U_SUCCESS(success))
				utc_displacement = 0.0;
		}
		// Imposta l'ora corrente sui TimerMarketReplayer ed estrae il primo tick da ciascuno di essi
		for (auto it = _trader_manager->getTimerReplayersSet(TraderManager::MarketReplayerManagerKey()).begin();
		     it != _trader_manager->getTimerReplayersSet(TraderManager::MarketReplayerManagerKey()).end(); ++it)
		{
			(*it)->setCurrentTime(_current_time);
			(*it)->reset();
			std::shared_ptr<MarketTick> tick(new MarketTick(*it));
			if (tick->readNextTick())
				addNewTick(tick);
		}
		// Cicla finche' non ha processato tutti i tick da tutti i replayer
		for (int cnt = 0; data_replayers_count > 0 && !_market_tick_queue.empty() && !_trader_manager->isStopped(); cnt++)
		{
			// Estrae il tick piu' vecchio dalla priority queue
			std::shared_ptr<const MarketTick> tick = _market_tick_queue.top();
			_market_tick_queue.pop();
			// Verifica se ha wrappato il giorno e azzera i volumi, nel caso
			if (fabs(floor((_current_time - utc_displacement) / U_MILLIS_PER_DAY) - floor(
				(tick->timestamp - utc_displacement) / U_MILLIS_PER_DAY)) > PRICE_EPSILON)
			{
				for (size_t i = 0; i < market_data.size(); i++)
					market_data[i].setVolume(0.);
			}
			// Il tick estratto da' l'ora attuale
			_current_time = tick->timestamp;
			// Se esiste il replayer (negli order ticks e' assente),
			// ne inserisce un altro preso dallo stesso replayer
			if (tick->replayer)
			{
				std::shared_ptr<MarketTick> new_tick(new MarketTick(tick->replayer));
				if (new_tick->readNextTick())
					addNewTick(new_tick);
					// Se il tick proviene da un data replayer che non riesce piu' a fornire un nuovo tick,
					// allora quel data replayer e' terminato e va rimosso dal conteggio
				else if (tick->type == MarketTick::DATA)
					data_replayers_count--;
			}
			// Processa il tick estratto prima
			switch (tick->type)
			{
			case MarketTick::DATA:
				// Aggiorna i prezzi
				if (tick->instrument->market_data)
				{
					MarketData* mkt_data = tick->instrument->market_data;
					mkt_data->setBookDepth(tick->getBookDepth());
					for (int i = 0; i < tick->getBookDepth(); ++i)
					{
						if (tick->bid_price[i] >= 0.)
							mkt_data->setBidPrice(i, tick->bid_price[i], _current_time);
						if (tick->ask_price[i] >= 0.)
							mkt_data->setAskPrice(i, tick->ask_price[i], _current_time);
						if (tick->ask_size[i] >= 0.)
							mkt_data->setAskSize(i, tick->ask_size[i], _current_time);
						if (tick->bid_size[i] >= 0.)
							mkt_data->setBidSize(i, tick->bid_size[i], _current_time);
					}
					if (tick->last_price >= 0.)
						mkt_data->setLastPrice(tick->last_price);
					if (tick->imbalance >= 0.)
						mkt_data->setImbalance(tick->imbalance);
					if (tick->auction_price >= 0.)
						mkt_data->setAuctionPrice(tick->auction_price);
					if (tick->open_price >= 0.)
						mkt_data->setOpenPrice(tick->open_price);
					if (tick->close_price >= 0.)
						mkt_data->setClosePrice(tick->close_price);
					if (tick->volume >= 0.)
						mkt_data->setVolume(tick->volume);
						// TODO: azzerare i volumi quando wrappa il giorno (bassa priorita')
					else if (tick->delta_volume > 0.)
						mkt_data->setVolume(mkt_data->volume + tick->delta_volume);
					if (tick->connection == MarketTick::CONNECTED)
						mkt_data->setMarketDataConnected(true, _current_time);
					else if (tick->connection == MarketTick::DISCONNECTED)
						mkt_data->setMarketDataConnected(false, _current_time);
					mkt_data->setUpdated(true, _current_time);
					tick->instrument->bidAskPricesSizesChanged();
					_trader_manager->updateMarketData(TraderManager::BaseClientKey());
				}
				break;
			case MarketTick::TIMER:
				// Esegue la callback del timer
				tick->handler();
				break;
			case MarketTick::ORDER:
				{
					const OpenOrder* open_order = _trader_manager->_order_manager->getOrderById(tick->order_id);
					if (open_order == nullptr)
						BOOST_LOG_TRIVIAL(error) << "Unexpected error: order id " << tick->order_id << " no longer exists";
					else
					{
						Instrument* instrument = open_order->instrument;
						// Dopo simulateOrder, open_order non va piu' usato perche' non e' piu' valido
						_trader_manager->_order_manager->simulateOrder(*open_order);
					}
				}
				break;
			case MarketTick::START_TRADING:
				_trader_manager->startTrading(TraderManager::MarketReplayerManagerKey());
				break;
			case MarketTick::STOP_TRADING:
				_trader_manager->stopTrading(TraderManager::MarketReplayerManagerKey());
				break;
			case MarketTick::CLOSE_POSITIONS:
				_trader_manager->closeAllPositions();
				break;
			}
			// Aggiorna il position manager
			_trader_manager->_position_manager->update();
			// Aggiorna il file di p&l
			if (pnl_file)
			{
				double unr_pnl = 0., rea_pnl = 0.;
				boost::ptr_vector<Instrument>& instruments = _trader_manager->getInstruments(
					TraderManager::InstrumentsMarketDataPositionsKey());
				for (size_t i = 0; i < instruments.size(); i++)
				{
					if (instruments[i].position)
					{
						unr_pnl += instruments[i].position->unrealized_pnl;
						rea_pnl += instruments[i].position->realized_pnl;
					}
				}
				if (prev_unr_pnl != unr_pnl || prev_rea_pnl != rea_pnl)
				{
					pnl_file << _trader_manager->getLocalDateTime() << ";" << unr_pnl << ";" << rea_pnl << ";" << (unr_pnl + rea_pnl)
						<< "\n";
					prev_unr_pnl = unr_pnl;
					prev_rea_pnl = rea_pnl;
				}
			}
			// Verifico se devo simulare la velocita' di replay
			double cur_time = Calendar::getNow();
			if (_trader_manager->getReplaySpeed() > 0.0)
			{
				if ((cur_time - _local_start_time) * _trader_manager->getReplaySpeed() < _current_time - _replay_start_time)
					boost::this_thread::sleep(boost::posix_time::milliseconds(
						(long long)((_current_time - _replay_start_time) / _trader_manager->getReplaySpeed() - cur_time +
							_local_start_time)));
			}
#ifdef QT_GUI_LIB
			// Verifica se deve aggiornare la gui
			double elapsed_ms = cur_time - gui_time_base;
			if (elapsed_ms >= _trader_manager->_update_gui_seconds * 1000)
			{
				gui_time_base = cur_time;
				_trader_manager->updateGui(TraderManager::MarketReplayerManagerKey());
			}
#endif
		}
	}
	// Chiude il file degli eseguiti
	_trader_manager->_order_manager->closeExeFile();
	// Chiude i file di debug
	_trader_manager->_indicator_manager->closeAllDebugFiles();
	_trader_manager->_strategy_manager->closeAllDebugFiles();
}

void MarketReplayerManager::addNewReplayer(BaseMarketReplayer* replayer)
{
	// Estrae il primo tick dal nuovo replayer e lo aggiunge alla coda
	replayer->reset();
	std::shared_ptr<MarketTick> tick(new MarketTick(replayer));
	if (tick->readNextTick())
		addNewTick(tick);
}

void MarketReplayerManager::addNewTick(std::shared_ptr<MarketTick> tick)
{
	// Serve per mantenere l'ordine di arrivo dei tick anche quando hanno lo stesso timestamp
	tick->order_if_same_timestamp = _tick_counter++;
	_market_tick_queue.push(tick);
}

BaseMarketReplayer* MarketReplayerManager::createMarketReplayer(const std::string& replayer, Instrument* instrument)
{
	if (replayer == "ib_bid_ask")
		return new IBBidAskMarketReplayer(instrument, _trader_manager->_calendar.get());
	if (replayer == "binary")
		return new IBBidAskMarketReplayer(instrument, _trader_manager->_calendar.get());
	//if (replayer == "sqlite")
	//	return new SqliteMarketReplayer(instrument, _trader_manager->_calendar.get());
	BOOST_LOG_TRIVIAL(error) << "Invalid market replayer: " << replayer;
	return nullptr;
}
