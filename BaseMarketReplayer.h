#pragma once

#include <memory>
#include <string>
#include <unicode/uversion.h>

namespace U_ICU_NAMESPACE
{
	class Calendar;
	class SimpleDateFormat;
};

struct OpenOrder;
struct Instrument;
struct MarketTick;

class BaseMarketReplayer
{
public:
	explicit BaseMarketReplayer(Instrument* instrument, U_ICU_NAMESPACE::Calendar* default_calendar);
	virtual ~BaseMarketReplayer(void);

	std::string error;

	virtual void reset() = 0;
	virtual bool readNextTick(MarketTick& tick) = 0;

	int getBookDepth() { return _book_depth; }

protected:
	Instrument* _instrument;
	std::shared_ptr<U_ICU_NAMESPACE::SimpleDateFormat> _ymd_hms_formatter;
	std::shared_ptr<U_ICU_NAMESPACE::SimpleDateFormat> _ymd_hm_formatter;
	std::shared_ptr<U_ICU_NAMESPACE::SimpleDateFormat> _chosen_record_formatter;
	double _replay_from_time;
	double _replay_to_time;
	int _book_depth;
};
