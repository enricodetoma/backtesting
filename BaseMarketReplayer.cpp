#include "BaseMarketReplayer.h"
#include <unicode/smpdtfmt.h>
using namespace U_ICU_NAMESPACE;
#include "Instrument.h"
#include "MarketTick.h"
#include "BaseStrategy.h"


BaseMarketReplayer::BaseMarketReplayer(Instrument* instrument, Calendar* default_calendar)
	: _instrument(instrument)
	  , _book_depth(1)
{
	if (instrument)
	{
		Calendar* calendar = nullptr;
		UErrorCode success = U_ZERO_ERROR;
		if (instrument->replay_time_zone && *instrument->replay_time_zone != 0)
		{
			TimeZone* tz = TimeZone::createTimeZone(instrument->replay_time_zone);
			if (*tz == TimeZone::getUnknown())
			{
				delete tz;
				BOOST_LOG_TRIVIAL(error) << "Invalid replay time zone: " << instrument->replay_time_zone;
			}
			else
				calendar = Calendar::createInstance(tz, success);
		}
		else if (default_calendar)
			calendar = default_calendar->clone();
		success = U_ZERO_ERROR;
		_ymd_hms_formatter.reset(new SimpleDateFormat(UnicodeString("y-MM-dd HH:mm:ss", -1, US_INV), success));
		_ymd_hm_formatter.reset(new SimpleDateFormat(UnicodeString("y-MM-dd HH:mm", -1, US_INV), success));
		if (calendar)
		{
			// Uno solo dei formatters puo' appropriarsi del calendar, gli altri devono fare clone()
			_ymd_hms_formatter->adoptCalendar(calendar->clone());
			_ymd_hm_formatter->adoptCalendar(calendar->clone());
		}
		_replay_from_time = 0.0;
		if (instrument->replay_from_time && *instrument->replay_from_time != 0)
		{
			success = U_ZERO_ERROR;
			_replay_from_time = _ymd_hms_formatter->parse(UnicodeString(instrument->replay_from_time, -1, US_INV), success);
			if (!U_SUCCESS(success))
			{
				success = U_ZERO_ERROR;
				_replay_from_time = _ymd_hm_formatter->parse(UnicodeString(instrument->replay_from_time, -1, US_INV), success);
				if (!U_SUCCESS(success))
					_replay_from_time = 0.0;
			}
		}
		_replay_to_time = 0.0;
		if (instrument->replay_to_time && *instrument->replay_to_time != 0)
		{
			success = U_ZERO_ERROR;
			_replay_to_time = _ymd_hms_formatter->parse(UnicodeString(instrument->replay_to_time, -1, US_INV), success);
			if (!U_SUCCESS(success))
			{
				success = U_ZERO_ERROR;
				_replay_to_time = _ymd_hm_formatter->parse(UnicodeString(instrument->replay_to_time, -1, US_INV), success);
				if (!U_SUCCESS(success))
					_replay_from_time = 0.0;
			}
		}
	}
}


BaseMarketReplayer::~BaseMarketReplayer(void)
{
}

//void BaseMarketReplayer::readCallback(double, int index_instrument, double bid_price, double ask_price, double bid_size, double ask_size, bool shortable)
//{
//	std::vector<MarketData> &market_data = _strategy->getMarketData();
//	market_data[index_instrument].bid_price[0] = bid_price;
//	market_data[index_instrument].bid_size[0] = bid_size;
//	market_data[index_instrument].ask_price[0] = ask_price;
//	market_data[index_instrument].ask_size[0] = ask_size;
//	market_data[index_instrument].shortable = shortable;
//
//	boost::ptr_vector<Instrument> &instruments = _strategy->getInstruments();
//	for (int i=0; i<_combos_to_calculate_market_data_for.size(); i++)
//	{
//		int index = _combos_to_calculate_market_data_for[i];
//		instruments[index].calculateComboMarketData(instruments, market_data[index], market_data);
//	}
//
//	_strategy->updateMarketData(timestamp);
//}
