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
	int x;
	int y;
	int index;
	int own;
};

const int max_square = 3;
const int size_square = 80;
const int invalid_index = -1;

typedef std::vector<Square> SquareList;

class Board
{
public:
	Board()
	{}
	~Board()
	{}

	void init()
	{
		int index = 0;
		for (int y=0;y<max_square;++y)
		{
			for (int x=0;x<max_square;++x)
			{
				Square s;
				s.x = (x+1)*size_square;
				s.y = (y+1)*size_square;
				s.index = index;
				s.own = Piece_None;
				square_list_.push_back(s);
				++index;
			}
		}
	};

	const SquareList & get_square_list() const
	{
		return square_list_;
	}

	int get_matched_index(const int x, const int y)
	{
		SquareList::const_iterator iter = square_list_.begin();
		int prev_x = 0;
		int prev_y = 0;
		for ( ; iter != square_list_.end(); ++iter)
		{
			Square s = (*iter);
			if ( x >= prev_x && x <= s.x &&
				y >= prev_y && y <= s.y )
			{
				return s.index;
			}

			if (s.index % max_square == max_square-1)
			{
				prev_x = 0;
				prev_y = s.y;
			}
			else
			{
				prev_x = s.x;
			}
		}
		return invalid_index;
	}

	void change_piece(const int index, const int piece)
	{
		SquareList::iterator iter = square_list_.begin();
		for ( ; iter != square_list_.end(); ++iter)
		{
			if (iter->index == index)
			{
				iter->own = piece;
				return;
			}
		}
	}

	void set_piece(const int piece)
	{
		piece_type_ = piece;
	}

	int get_piece() const
	{
		return piece_type_;
	}

	void set_id(const int id)
	{
		id_ = id;
	}

	int get_id() const
	{
		return id_;
	}

	void print()
	{
#if 0
		Square_List::const_iterator iter = square_list_.begin();
		for ( ; iter != square_list_.end(); ++iter)
		{
			Square s = (*iter);
			std::cout << "["<< s.x << ", " << s.y << "] ";
			if (s.index % max_square == max_square-1)
				std::cout << std::endl;
		}
#endif
	}

private:
	SquareList square_list_;
	int piece_type_;
	int id_;

};
