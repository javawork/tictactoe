#pragma once

#include <vector>

enum
{
	Piece_None,
	Piece_White,
	Piece_Dark,
};

struct Square
{
	int index;
	int piece;
};

const int max_square = 3;
const int size_square = 80;
const int invalid_index = -1;

typedef std::vector<Square> SquareList;

class Board
{
public:
	void init()
	{
		square_list_.clear();
		int index = 0;
		for (int y=0;y<max_square;++y)
		{
			for (int x=0;x<max_square;++x)
			{
				Square s;
				s.index = index;
				s.piece = Piece_None;
				square_list_.push_back(s);
				++index;
			}
		}
	};

	const SquareList & get_square_list() const
	{
		return square_list_;
	}

	void change_piece(const int index, const int piece)
	{
		SquareList::iterator iter = square_list_.begin();
		for ( ; iter != square_list_.end(); ++iter)
		{
			if (iter->index == index)
			{
				iter->piece = piece;
				return;
			}
		}
	}

	bool check_winner(const int piece)
	{
		char score[max_square] = {0,};
		for each ( Square s in square_list_)
		{
			if (s.piece != piece)
				continue;

			++score[s.index/max_square];
		}

		for each ( char s in score )
		{
			if ( s >= 3 )
				return true;
		}

		memset(score, 0, max_square);
		for each ( Square s in square_list_)
		{
			if (s.piece != piece)
				continue;

			++score[s.index%max_square];
		}

		for each ( char s in score )
		{
			if ( s >= 3 )
				return true;
		}

		if ( square_list_[0].piece == piece && 
			square_list_[4].piece == piece && 
			square_list_[8].piece == piece )
			return true;

		if ( square_list_[2].piece == piece && 
			square_list_[4].piece == piece && 
			square_list_[6].piece == piece )
			return true;

		return false;
	}

private:
	SquareList square_list_;

};
