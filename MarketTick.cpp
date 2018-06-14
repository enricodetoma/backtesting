#include "MarketTick.h"
#include "BaseMarketReplayer.h"

MarketTick::MarketTick(BaseMarketReplayer* replayer_)
	: replayer(replayer_)
	  , type(DATA)
	  , instrument(nullptr)
	  , timestamp(0.)
	  , order_if_same_timestamp(0)
	  , bid_size(replayer_->getBookDepth(), -1.)
	  , bid_price(replayer_->getBookDepth(), -1.)
	  , ask_price(replayer_->getBookDepth(), -1.)
	  , ask_size(replayer_->getBookDepth(), -1.)
	  , last_price(-1.)
	  , volume(-1.)
	  , delta_volume(-1.)
	  , imbalance(-1.)
	  , auction_price(-1.)
	  , open_price(-1.)
	  , close_price(-1.)
	  , shortable(false)
	  , connection(NO_CHANGE)
	  , order_id(0)
	  , book_depth(replayer_->getBookDepth())
{
}

void MarketTick::setBookDepth(int _book_depth)
{
	book_depth = _book_depth;
	bid_size.resize(book_depth, -1.);
	bid_price.resize(book_depth, -1.);
	ask_price.resize(book_depth, -1.);
	ask_size.resize(book_depth, -1.);
}

bool MarketTick::readNextTick()
{
	return replayer ? replayer->readNextTick(*this) : false;
}
