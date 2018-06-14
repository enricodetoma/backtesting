#pragma once

#include <string>
#include <vector>
#include <queue>
#include <set>
#include <boost/ptr_container/ptr_vector.hpp>
#include <unicode/uversion.h>
#include "BaseMarketReplayer.h"
#include "BaseStrategy.h"
#include "MarketTick.h"

namespace U_ICU_NAMESPACE
{
	class Calendar;
	class SimpleDateFormat;
};
class TraderManager;

struct PriorityCompareMarketTick
{
	bool operator()(std::shared_ptr<MarketTick> const& l, std::shared_ptr<MarketTick> const& r)
	{
		return l->timestamp > r->timestamp || (l->timestamp == r->timestamp && l->order_if_same_timestamp > r->
			order_if_same_timestamp);
	}
};

class MarketReplayerManager
{
	friend class TraderManager;

public:
	MarketReplayerManager(TraderManager* trader_manager, const std::string& default_replayer,
	                      const std::string& pnl_file_name);
	~MarketReplayerManager(void);

	void replay();
	double getCurrentTime() const { return _current_time; }
	void addNewReplayer(BaseMarketReplayer* replayer);
	void addNewTick(std::shared_ptr<MarketTick> tick);

	std::set<std::string> _registered_debug_files;

private:
	unsigned long _tick_counter;
	double _current_time;
	TraderManager* _trader_manager;
	std::string _default_replayer;
	std::string _pnl_file_name;
	std::vector<size_t> _combos_to_calculate_market_data_for;
	boost::ptr_vector<boost::nullable<BaseMarketReplayer>> _replayers;
	std::priority_queue<std::shared_ptr<MarketTick>, std::vector<std::shared_ptr<MarketTick>>, PriorityCompareMarketTick> _market_tick_queue;
	double _local_start_time;
	double _replay_start_time;

	BaseMarketReplayer* createMarketReplayer(const std::string& replayer, Instrument* instrument);
};
