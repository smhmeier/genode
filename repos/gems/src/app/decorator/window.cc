/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "window.h"


void Decorator::Window::draw(Decorator::Canvas_base &canvas, Ref const &win_ref,
                             Decorator::Rect clip, Draw_behind_fn const &draw_behind_fn) const
{
	Clip_guard clip_guard(canvas, clip);

	Rect rect = outer_geometry();
	Area corner(_corner_size, _corner_size);

	Point p1 = rect.p1();
	Point p2 = rect.p2();

	if (_has_alpha)
		draw_behind_fn.draw_behind(canvas, win_ref, canvas.clip());

	_draw_corner(canvas, Rect(p1, corner), _border_size, true, true,
	             _window_elem_attr(Element::TOP_LEFT));

	_draw_corner(canvas, Rect(Point(p1.x, p2.y - _corner_size + 1), corner),
	             _border_size, true, false,
	             _window_elem_attr(Element::BOTTOM_LEFT));

	_draw_corner(canvas, Rect(Point(p2.x - _corner_size + 1, p1.y), corner),
	             _border_size, false, true,
	             _window_elem_attr(Element::TOP_RIGHT));

	_draw_corner(canvas, Rect(Point(p2.x - _corner_size + 1, p2.y - _corner_size + 1), corner),
	             _border_size, false, false,
	             _window_elem_attr(Element::BOTTOM_RIGHT));

	_draw_raised_box(canvas, Rect(Point(p1.x + _corner_size, p1.y),
	                              Area(rect.w() - 2*_corner_size, _border_size)),
	                              _window_elem_attr(Element::TOP));

	_draw_raised_box(canvas, Rect(Point(p1.x + _corner_size, p2.y - _border_size + 1),
	                              Area(rect.w() - 2*_corner_size, _border_size)),
	                              _window_elem_attr(Element::BOTTOM));

	_draw_raised_box(canvas, Rect(Point(p1.x, p1.y + _corner_size),
	                              Area(_border_size, rect.h() - 2*_corner_size)),
	                              _window_elem_attr(Element::LEFT));

	_draw_raised_box(canvas, Rect(Point(p2.x - _border_size + 1, p1.y + _corner_size),
	                              Area(_border_size, rect.h() - 2*_corner_size)),
	                              _window_elem_attr(Element::RIGHT));

	Rect controls_rect(Point(p1.x + _border_size, p1.y + _border_size),
	                   Area(rect.w() - 2*_border_size, _title_height));


	/*
	 * Draw left controls from left to right
	 */

	Control::Align title_align = Control::ALIGN_CENTER;

	Point left_pos = controls_rect.p1();

	for (unsigned i = 0; i < _controls.num(); i++) {

		Control control = _controls.control(i);

		/* left controls end when we reach the title */
		if (control.type() == Control::TYPE_TITLE) {
			title_align = control.align();
			break;
		}

		_draw_window_control(canvas, Rect(left_pos, _icon_size), control);
		left_pos = left_pos + Point(_icon_size.w, 0);
	}

	/*
	 * Draw right controls from right to left
	 */

	Point right_pos = controls_rect.p1() + Point(controls_rect.w() - _icon_size.w, 0);

	if (_controls.num() > 0) {
		for (int i = _controls.num() - 1; i >= 0; i--) {

			Control control = _controls.control(i);

			/* stop when reaching the title */
			if (control.type() == Control::TYPE_TITLE)
				break;

			/* detect overlap with left controls */
			if (right_pos.x <= left_pos.x)
				break;

			_draw_window_control(canvas, Rect(right_pos, _icon_size), control);
			right_pos = right_pos + Point(-_icon_size.w, 0);
		}
	}

	/*
	 * Draw title between left and right controls
	 */

	Rect title_rect(left_pos, Area(right_pos.x - left_pos.x + _icon_size.w,
	                               _title_height));

	_draw_title_box(canvas, title_rect, _window_elem_attr(Element::TITLE));

	char const * const text = _title.string();

	Area const label_area(default_font().string_width(text).decimal(),
	                      default_font().bounding_box().h);

	/*
	 * Position the text in the center of the window.
	 */
	int const title_yshift = element(Element::TITLE).pressed() ? 0 : -1;
	Point const window_centered_text_pos = controls_rect.center(label_area)
	                                     + Point(0, title_yshift);

	/*
	 * Horizontal position of the title text
	 */
	int x = window_centered_text_pos.x;

	/*
	 * If the title bar is narrower than three times the label but the text
	 * still fits in the title bar, we gradually change the text position
	 * towards the center of the title bar. If the text fits twice in the
	 * title bar, it is centered within the title bar.
	 */
	if (label_area.w <= title_rect.w() && label_area.w*3 > title_rect.w()) {

		int ratio = ((label_area.w*3 - title_rect.w()) << 8) / title_rect.w();

		if (ratio > 255)
			ratio = 255;

		Point const titlebar_centered_text_pos =
			title_rect.center(label_area) - Point(0, 1);

		x = (titlebar_centered_text_pos.x*ratio +
		     window_centered_text_pos.x*(255 - ratio)) >> 8;
	}

	/* minimum distance between the title text and the title border */
	int const min_horizontal_padding = 4;

	/*
	 * Consider non-default title alignments
	 */
	if (title_align == Control::ALIGN_LEFT)
		x = title_rect.x1() + min_horizontal_padding;

	if (title_align == Control::ALIGN_RIGHT)
		x = title_rect.x2() - label_area.w - min_horizontal_padding;

	/*
	 * If the text does not fit into the title bar, align it to the left
	 * border of the title bar to show the first part.
	 */
	if (label_area.w + 2*min_horizontal_padding > title_rect.w())
		x = title_rect.x1() + min_horizontal_padding;

	{
		Rect const title_content_rect = Rect::compound(title_rect.p1() + Point(1, 1),
		                                               title_rect.p2() - Point(1, 1));

		Clip_guard clip_guard(canvas, title_content_rect);

		Point const text_pos(x, window_centered_text_pos.y);

		canvas.draw_text(text_pos + Point(1, 1), default_font(),
		                 Color(0, 0, 0, 128), text);

		Color const title_color = element(Element::TITLE).color();

		auto const alpha = Genode::uint8_t((2*255 + title_color.r) / 3);
		canvas.draw_text(text_pos, default_font(),
		                 Color { 255, 255, 255, alpha }, text);
	}
}


bool Decorator::Window::update(Genode::Xml_node const &window_node)
{
	bool updated = false;

	/*
	 * Detect geometry changes
	 */
	Rect new_geometry = Rect::from_xml(window_node);
	if (new_geometry.p1() != geometry().p1()
	 || new_geometry.p2() != geometry().p2()) {

		geometry(new_geometry);

		_gui_views_up_to_date = false;

		updated |= true;
	}

	_focused   = window_node.attribute_value(  "focused", false);
	_has_alpha = window_node.attribute_value("has_alpha", false);

	Window_title const title =
		window_node.attribute_value("title", Window_title("<untitled>"));
	updated |= !(title == _title);
	_title = title;

	/* update color on title change as the title is used as policy selector */
	Color const base_color = _config.base_color(_title);
	updated |= _base_color != base_color;
	_base_color = base_color;

	int const gradient_percent = _config.gradient_percent(_title);
	updated |= _gradient_percent != gradient_percent;
	_gradient_percent = gradient_percent;

	/* update window-control configuration */
	Controls new_controls;
	for (unsigned i = 0; i < _config.num_window_controls(); i++) {

		Control window_control = _config.window_control(i);

		switch (window_control.type()) {

		case Control::TYPE_CLOSER:
		case Control::TYPE_MAXIMIZER:
		case Control::TYPE_MINIMIZER:
		case Control::TYPE_UNMAXIMIZER:
		case Control::TYPE_UNDEFINED:
			{
				char const * const attr =
					Control::type_name(window_control.type());

				if (window_node.attribute_value(attr, false))
					new_controls.add(window_control);
				break;
			}

		case Control::TYPE_TITLE:
			new_controls.add(window_control);
			break;
		};
	}

	updated |= (new_controls != _controls);
	_controls = new_controls;

	auto with_highlight = [&] (auto const &fn)
	{
		window_node.with_sub_node("highlight",
			[&] (Xml_node const &highlight) { fn(highlight); },
			[&] { fn(Xml_node("<highlight/>")); });
	};

	with_highlight([&] (Xml_node const &highlight) {

		for (unsigned i = 0; i < num_elements(); i++) {

			Window_element &element = _elements[i];

			Window_element::State state = element.state();

			state.highlighted = false;
			state.pressed     = false;
			state.focused     = _focused;

			highlight.for_each_sub_node([&] (Xml_node const &node) {

				if (node.type() == element.type_name()) {
					state.highlighted = true;

					if (node.attribute_value("pressed", false))
						state.pressed = true;
				}
			});

			updated |= element.apply_state(state);
		}
	});

	return updated;
}


Decorator::Window_base::Hover Decorator::Window::hover(Point abs_pos) const
{
	Hover hover;

	if (!outer_geometry().contains(abs_pos))
		return hover;

	hover.window_id = id();

	unsigned const x = abs_pos.x - outer_geometry().x1(),
	               y = abs_pos.y - outer_geometry().y1();

	Area const area = outer_geometry().area;

	bool const at_border = x <  _border_size
	                    || x >= area.w - _border_size
	                    || y <  _border_size
	                    || y >= area.h - _border_size;

	if (at_border) {

		hover.left_sizer   = (x < _corner_size);
		hover.top_sizer    = (y < _corner_size);
		hover.right_sizer  = (x >= area.w - _corner_size);
		hover.bottom_sizer = (y >= area.h - _corner_size);

	} else {

		Point const titlbar_pos(_border_size, _border_size);

		hover.title       = false;
		hover.closer      = false;
		hover.minimizer   = false;
		hover.maximizer   = false;
		hover.unmaximizer = false;

		/*
		 * Check if pointer is located at the title bar
		 */
		if (y < _border_size + _title_height) {

			Control hovered_control = Control(Control::TYPE_TITLE, Control::ALIGN_CENTER);

			/* check left controls */
			{
				Point pos = titlbar_pos;
				for (unsigned i = 0; i < _controls.num(); i++) {

					/* controls end when we reach the title */
					if (_controls.control(i).type() == Control::TYPE_TITLE)
						break;

					if (Rect(pos, _icon_size).contains(Point(x + _border_size, y)))
						hovered_control = _controls.control(i);

					pos = pos + Point(_icon_size.w, 0);
				}
			}

			/* check right controls */
			if (_controls.num() > 0) {

				Point pos = titlbar_pos +
				            Point(area.w - _border_size - _icon_size.w, 0);

				for (int i = _controls.num() - 1; i >= 0; i--) {

					/* controls end when we reach the title */
					if (_controls.control(i).type() == Control::TYPE_TITLE)
						break;

					if (Rect(pos, _icon_size).contains(Point(x + _border_size, y)))
						hovered_control = _controls.control(i);

					pos = pos + Point(-_icon_size.w, 0);
				}
			}

			switch (hovered_control.type()) {
			case Control::TYPE_CLOSER:      hover.closer      = true; break;
			case Control::TYPE_MAXIMIZER:   hover.maximizer   = true; break;
			case Control::TYPE_MINIMIZER:   hover.minimizer   = true; break;
			case Control::TYPE_UNMAXIMIZER: hover.unmaximizer = true; break;
			case Control::TYPE_TITLE:       hover.title       = true; break;
			case Control::TYPE_UNDEFINED:                             break;
			};
		}
	}

	return hover;
}


void Decorator::Window_element::animate()
{
	_r.animate();
	_g.animate();
	_b.animate();

	/* keep animation running until the destination values are reached */
	Animator::Item::animated(_r != _r.dst() || _g != _g.dst() || _b != _b.dst());
}
