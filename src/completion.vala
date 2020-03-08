/*
 * This file is part of GNOME LaTeX. It was adapted for use with Gummi.
 *
 * Copyright © 2010-2012 Sébastien Wilmet (original code)
 *             2020 Nikita Nikulsin (modifications for use with Gummi)
 *
 * GNOME LaTeX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNOME LaTeX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNOME LaTeX.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Sébastien Wilmet
 *          Pieter Pareit
 *          Nikita Nikulsin
 */

using Gtk;

extern bool config_get_boolean(string group, string key);
extern int config_get_integer(string group, string key);
extern const string GUMMI_DATA;

public class GuCompletion : GLib.Object, SourceCompletionProvider {
	struct CompletionCommand {
		string name;
		string? package;
		CompletionArgument[] args;
	}

	struct CompletionArgument {
		string label;
		bool optional;
		CompletionChoice[] choices;
	}

	struct CompletionChoice {
		string name;
		string? package;
		string? insert;
		string? insert_after;
	}

	struct ArgumentContext {
		string cmd_name;
		string arg_contents;

		// After the command name, list the arguments types encountered.
		// The last one is the argument where the cursor is.
		// The value is 'true' for an optional argument.
		Gee.ArrayList<bool> args_types;
	}
	
	private Gee.HashMap<string, CompletionCommand?> _commands;
	// contains only environments that have extra info
	private Gee.HashMap<string, CompletionChoice?> _environments;
	private List<SourceCompletionItem> _proposals;
	private static GuCompletion _instance = null;
	
	// While parsing the XML file, keep track of current command/argument/choice.
	private CompletionCommand _current_command;
	private CompletionArgument _current_arg;
	private CompletionChoice _current_choice;
	
	private Gdk.Pixbuf? _icon_cmd;
	private Gdk.Pixbuf? _icon_choice;
	private Gdk.Pixbuf? _icon_package_required;
	
	private GuCompletion() {
		// icons
		_icon_cmd = new Gdk.Pixbuf.from_file_at_size(Path.build_filename(GUMMI_DATA, "icons", "green-circle.png"), 16, 16);
		_icon_choice = new Gdk.Pixbuf.from_file_at_size(Path.build_filename(GUMMI_DATA, "icons", "gray-square.png"), 16, 16);
		_icon_package_required = IconTheme.get_default().load_icon("dialog-warning", 16, 0);
		
		_commands = new Gee.HashMap<string, CompletionCommand?>();
		_environments = new Gee.HashMap<string, CompletionChoice?>();

		File file = File.new_for_path(Path.build_filename(GUMMI_DATA, "misc", "completion.xml"));

		string? contents = load_file(file);
		if (contents == null) return;

		try {
			MarkupParser parser = { parser_start, parser_end, parser_text, null, null };
			MarkupParseContext context = new MarkupParseContext(parser, 0, this, null);
			context.parse(contents, -1);
			_proposals.sort((CompareFunc)compare_proposals);
		} catch (GLib.Error e) {
			warning("Impossible to load completion data: %s", e.message);
		}
	}
	
	private static int compare_proposals(SourceCompletionItem a, SourceCompletionItem b) {
		return a.text.collate (b.text);
	}
	
	private void parser_start(MarkupParseContext context, string name, string[] attr_names, string[] attr_values) throws MarkupError {
		switch (name) {
			case "commands":
				break;

			case "command":
				parser_add_command(attr_names, attr_values);
				break;

			case "argument":
				parser_add_argument(attr_names, attr_values);
				break;

			case "choice":
				parser_add_choice(attr_names, attr_values);
				break;

			// insert and insert_after don't contain any attributes, but
			// contain content, which is parsed in parser_text()
			case "insert":
			case "insert_after":
				break;

			// not yet supported
			case "placeholder":
			case "component":
				break;

			default:
				throw new MarkupError.UNKNOWN_ELEMENT("unknown element \"" + name + "\"");
		}
	}
	
	private void parser_add_command(string[] attr_names, string[] attr_values) throws MarkupError {
		_current_command = CompletionCommand();

		for (int attr_num = 0; attr_num < attr_names.length; attr_num++) {
			switch (attr_names[attr_num]) {
				case "name":
					_current_command.name = "\\" + attr_values[attr_num];
					break;

				case "package":
					_current_command.package = attr_values[attr_num];
					break;

				// not yet supported
				case "environment":
					break;

				default:
					throw new MarkupError.UNKNOWN_ATTRIBUTE("unknown command attribute \"" + attr_names[attr_num] + "\"");
			}
		}
	}
	
	private void parser_add_argument(string[] attr_names, string[] attr_values) throws MarkupError {
		_current_arg = CompletionArgument();
		_current_arg.optional = false;

		for (int attr_num = 0; attr_num < attr_names.length; attr_num++) {
			switch (attr_names[attr_num]) {
				case "label":
					_current_arg.label = attr_values[attr_num];
					break;

				case "type":
					_current_arg.optional = attr_values[attr_num] == "optional";
					break;

				default:
					throw new MarkupError.UNKNOWN_ATTRIBUTE("unknown argument attribute \"" + attr_names[attr_num] + "\"");
			}
		}
	}
	
	private void parser_add_choice(string[] attr_names, string[] attr_values) throws MarkupError {
		_current_choice = CompletionChoice();

		for (int attr_num = 0; attr_num < attr_names.length; attr_num++) {
			switch (attr_names[attr_num]) {
				case "name":
					_current_choice.name = attr_values[attr_num];
					break;

				case "package":
					_current_choice.package = attr_values[attr_num];
					break;

				default:
					throw new MarkupError.UNKNOWN_ATTRIBUTE("unknown choice attribute \"" + attr_names[attr_num] + "\"");
			}
		}
	}
	
	private void parser_end(MarkupParseContext context, string name) throws MarkupError {
		switch (name) {
			case "command":
				Gdk.Pixbuf pixbuf = _current_command.package != null ? _icon_package_required : _icon_cmd;

				SourceCompletionItem item = new SourceCompletionItem(_current_command.name, get_command_text_to_insert(_current_command), pixbuf, get_command_info(_current_command));
				_proposals.prepend (item);

				// We don't need to store commands that have no arguments,
				// they are only in _proposals, it's sufficient.
				if (0 < _current_command.args.length) _commands[_current_command.name] = _current_command;
				break;

				case "argument":
					_current_command.args += _current_arg;
					break;

				case "choice":
					_current_arg.choices += _current_choice;
					if (_current_choice.insert != null || _current_choice.insert_after != null) _environments[_current_choice.name] = _current_choice;
					break;
		}
	}
	
	private void parser_text(MarkupParseContext context, string text, size_t text_len) throws MarkupError {
		switch (context.get_element()) {
			case "insert":
				_current_choice.insert = text;
				break;

			case "insert_after":
				_current_choice.insert_after = text;
				break;
		}
	}
	
	private string get_command_text_to_insert(CompletionCommand cmd) {
		string text_to_insert = cmd.name;
		foreach (CompletionArgument arg in cmd.args)
			if (!arg.optional) text_to_insert += "{}";
		return text_to_insert;
	}
	
	public static GuCompletion get_default() {
		if (_instance == null) _instance = new GuCompletion();
		
		return _instance;
	}
	
	public void add_ref_choice(string choice) {
		CompletionCommand cmd_ref = _commands["\\ref"];
		CompletionCommand cmd_eqref = _commands["\\eqref"];
		CompletionCommand cmd_pageref = _commands["\\pageref"];
		foreach (CompletionChoice cc in cmd_ref.args[0].choices)
			if (cc.name == choice) return;
		CompletionChoice cchoice = CompletionChoice();
		cchoice.name = choice;
		cmd_ref.args[0].choices += cchoice;
		cmd_eqref.args[0].choices = cmd_ref.args[0].choices;
		cmd_pageref.args[0].choices = cmd_ref.args[0].choices;
		_commands["\\ref"] = cmd_ref;
		_commands["\\eqref"] = cmd_eqref;
		_commands["\\pageref"] = cmd_pageref;
	}
	
	public void add_citation_choice(string choice) {
		CompletionCommand cmd_cite = _commands["\\cite"];
		foreach (CompletionChoice cc in cmd_cite.args[0].choices)
			if (cc.name == choice) return;
		CompletionChoice cchoice = CompletionChoice();
		cchoice.name = choice;
		cmd_cite.args[0].choices += cchoice;
		_commands["\\cite"] = cmd_cite;
	}
	
	public void add_environment(string env, string? package) {
		CompletionCommand cmd_begin = _commands["\\begin"];
		foreach (CompletionChoice cc in cmd_begin.args[0].choices)
			if (cc.name == env) return;
		CompletionChoice choice = CompletionChoice();
		choice.name = env;
		choice.package = package;
		cmd_begin.args[0].choices += choice;
		_commands["\\begin"] = cmd_begin;
	}
	
	public void add_command(string name, string[] arg_names, bool first_arg_opt, string? package) {
		CompletionCommand cmd = CompletionCommand();
		CompletionArgument[] args = {};
		cmd.name = name;
		cmd.package = package;
		foreach (string arg in arg_names) {
			CompletionArgument ca = CompletionArgument();
			ca.label = arg;
			args += ca;
		}
		if (first_arg_opt && arg_names.length != 0) args[0].optional = true;
		cmd.args = args;
		
		for (int i = 0; i < _proposals.length(); i++)
			if (_proposals.nth_data(i).label == name) _proposals.remove_link(_proposals.nth(i));
		Gdk.Pixbuf pixbuf = package != null ? _icon_package_required : _icon_cmd;
		SourceCompletionItem item = new SourceCompletionItem(cmd.name, get_command_text_to_insert(cmd), pixbuf, get_command_info(cmd));
		_proposals.prepend(item);
		if (arg_names.length != 0) _commands[name] = cmd;
	}
	
	public string get_name() {
		return "LaTeX";
	}
	
	public SourceCompletionActivation get_activation() {
		var activation = SourceCompletionActivation.USER_REQUESTED;

		if (config_get_boolean("Editor", "interactive_completion")) activation |= SourceCompletionActivation.INTERACTIVE;

		return activation;
	}
	
	public bool match(SourceCompletionContext context) {
		TextIter iter;

		if (!context.get_iter(out iter))
		    return false;

		// if text selected, NO completion
		TextBuffer buf = iter.get_buffer();
		if (buf.has_selection)
		    return false;

		return true;
	}
	
	public bool get_start_iter(SourceCompletionContext context, SourceCompletionProposal proposal, out TextIter iter) {
		iter = {};
		string? cmd = get_latex_command_at_iter(context.iter);

		// In a LaTeX command argument, use the default implementation.
		if (cmd == null) return false;

		// Custom implementation when in a LaTeX command name.
		iter = context.iter;

		TextIter prev = iter;
		if (prev.backward_char() && prev.get_char() == '\\') {
			iter = prev;
			return true;
		}

		if (!iter.starts_word()) iter.backward_visible_word_start();

		prev = iter;
		if (prev.backward_char() && prev.get_char() == '\\') iter = prev;

		return true;
	}
	
	public void populate(SourceCompletionContext context) {
		TextIter iter;

		if (!context.get_iter(out iter)) {
			show_no_proposals(context);
			return;
		}

		// Is the cursor in a command name?
		string? cmd = get_latex_command_at_iter(iter);
		
		if (cmd != null) {
			populate_command(context, cmd);
			return;
		}
		
		// Is the cursor in a command's argument?
		ArgumentContext info;
		bool in_arg = in_latex_command_argument(iter, out info);

		if (in_arg) {
			populate_argument(context, info);
			return;
		}

    // Neither in a command name, nor an argument.
		if (is_user_request(context)) show_all_proposals(context);
		else show_no_proposals(context);
	}
	
	private void populate_command(SourceCompletionContext context, string cmd) {
		if (!is_user_request(context)) {
			uint min_nb_chars = config_get_integer("Editor", "minchar");

			if (cmd.length <= min_nb_chars) {
				show_no_proposals(context);
				return;
			}
		}

		if (cmd == "\\") {
			show_all_proposals(context);
			return;
		}

		show_filtered_proposals(context, _proposals, cmd);
	}
	
	private void populate_argument(SourceCompletionContext context, ArgumentContext info) {
		// invalid argument's command
		if (!_commands.has_key(info.cmd_name)) {
			show_no_proposals(context);
			return;
		}

		unowned List<SourceCompletionItem> proposals_to_filter = get_argument_proposals(info);

		if (proposals_to_filter == null) {
			show_no_proposals(context);
			return;
		}

		show_filtered_proposals(context, proposals_to_filter, info.arg_contents);
	}
	
	private unowned List<SourceCompletionItem>? get_argument_proposals(ArgumentContext arg_context) {
		return_val_if_fail(_commands.has_key(arg_context.cmd_name), null);

		CompletionCommand cmd = _commands[arg_context.cmd_name];
		string cmd_info = get_command_info(cmd);

		int arg_num = get_argument_num(cmd.args, arg_context.args_types);
		if (arg_num == -1) return null;

		CompletionArgument arg = cmd.args[arg_num - 1];
		unowned List<SourceCompletionItem> items = null;

		foreach (CompletionChoice choice in arg.choices) {
			Gdk.Pixbuf pixbuf;
			string? arg_info = null;
			if (choice.package != null) {
				pixbuf = _icon_package_required;
				arg_info = cmd_info + "\nPackage: " + choice.package;
			} else pixbuf = _icon_choice;

			SourceCompletionItem item = new SourceCompletionItem(choice.name, choice.name, pixbuf, arg_info ?? cmd_info);
			items.prepend(item);
		}

		if (items == null) return null;

		items.sort((CompareFunc)compare_proposals);
		return items;
	}
	
	// Get the command information: the prototype, and the package required if a package
	// is required. In the prototype, the current argument ('cur_arg') is in bold.
	// By default, no argument is in bold.
	private string get_command_info(CompletionCommand cmd, int cur_arg = -1) {
		string info = cmd.name;
		int arg_num = 1;
		foreach (CompletionArgument arg in cmd.args) {
			if (arg_num == cur_arg) info += "<b>";

			if (arg.optional) info += "[" + arg.label + "]";
			else info += "{" + arg.label + "}";

			if (arg_num == cur_arg) info += "</b>";

			arg_num++;
		}

		if (cmd.package != null) info += "\nPackage: " + cmd.package;

		return info;
	}
	
	/* Get argument number (begins at 1).
	 * 'all_args': all the possible arguments of a LaTeX command.
	 * 'args': the encounter arguments, beginning just after the command name.
	 * Returns -1 if it doesn't match.
	 */
	private int get_argument_num(CompletionArgument[] all_args, Gee.ArrayList<bool> args) {
		if (all_args.length < args.size) return -1;

		int num = 0;
		foreach (bool arg in args) {
			while (true) {
				if (all_args.length <= num) return -1;

				if (all_args[num].optional == arg) break;

				// missing non-optional argument
				else if (!all_args[num].optional) return -1;

				num++;
			}
			num++;
		}

		// first = 1
		return num;
	}
	
	private bool in_latex_command_argument(TextIter iter, out ArgumentContext info) {
		info = ArgumentContext();
		info.cmd_name = null;
		info.arg_contents = null;
		info.args_types = new Gee.ArrayList<bool>();

		string text = get_text_line_to_iter(iter);
		int last_index = text.length;
		int cur_index = last_index;
		unichar cur_char;

		/* Fetch the argument's contents */
		while (true) {
			if (!text.get_prev_char(ref cur_index, out cur_char)) return false;
			
			// If a closing bracket is encountered before an opening bracket, then not in command argument
			bool closing_bracket = cur_char == '}' || cur_char == ']';
			if (closing_bracket && !char_is_escaped(text, cur_index)) return false;

			// End of the argument's contents.
			bool delimiter = cur_char == '{' || cur_char == '[' || cur_char == ',';
			if (delimiter && !char_is_escaped(text, cur_index)) {
				info.arg_contents = text[cur_index + 1 : last_index];
				if (cur_char != ',') info.args_types.insert(0, cur_char == '[');
				else while (true) {
					if (!text.get_prev_char(ref cur_index, out cur_char)) return false;
					// Commas can be encountered outside of command arguments, so check for closing brackets
					closing_bracket = cur_char == '}' || cur_char == ']';
					if (closing_bracket && !char_is_escaped(text, cur_index)) return false;
					bool opening_bracket = cur_char == '{' || cur_char == '[';
					if (opening_bracket && !char_is_escaped(text, cur_index)) {
						info.args_types.insert(0, cur_char == '[');
						break;
					}
				}
				break;
			}
		}

		/* Traverse the previous arguments, and find the command name */
		bool in_prev_arg = false;
		unichar prev_arg_opening_bracket = '{';

		while (true) {
			if (!text.get_prev_char(ref cur_index, out cur_char)) return false;

			// In the contents of a previous argument.
			if (in_prev_arg) {
				if (cur_char == prev_arg_opening_bracket) in_prev_arg = char_is_escaped(text, cur_index);
			} else if (cur_char == '}' || cur_char == ']') {
				// Maybe the end of a previous argument.
				if (char_is_escaped(text, cur_index)) return false;

				in_prev_arg = true;
				prev_arg_opening_bracket = cur_char == '}' ? '{' : '[';

				info.args_types.insert(0, cur_char == ']');
			} else if (cur_char.isalpha() || cur_char == '*') {
				// Maybe the last character of the command name.
				info.cmd_name = get_latex_command_at_index(text, cur_index + 1);
				return info.cmd_name != null;
			} else if (!cur_char.isspace()) return false; // Spaces are allowed between arguments.
		}
	}
	
	private bool is_user_request(SourceCompletionContext context) {
		return context.activation == SourceCompletionActivation.USER_REQUESTED;
	}
	
	// It has the same effect as returning false in match().
	private void show_no_proposals(SourceCompletionContext context) {
		context.add_proposals((SourceCompletionProvider)this, null, true);
	}

	private void show_all_proposals(SourceCompletionContext context) {
		context.add_proposals((SourceCompletionProvider)this, _proposals, true);
	}
	
	private void show_filtered_proposals(SourceCompletionContext context, List<SourceCompletionItem> proposals_to_filter, string? prefix) {
		// No filtering needed.
		if (prefix == null || prefix == "")	{
			context.add_proposals((SourceCompletionProvider)this, proposals_to_filter, true);
			return;
		}

		// TODO this is a O(n) time complexity. This could be reduced to a O(log n) by
		// using a GSequence for example, like it is done by the words completion provider
		// in GtkSourceView.
		List<SourceCompletionItem> filtered_proposals = null;
		foreach (SourceCompletionItem item in proposals_to_filter)
			if (item.text.has_prefix(prefix)) filtered_proposals.prepend(item);

		// Since we have prepend items we must reverse the list to keep the proposals
		// in ascending order.
		if (filtered_proposals != null) filtered_proposals.reverse();

		// No match, show a message so the completion widget doesn't disappear.
		else {
			SourceCompletionItem dummy_proposal = new SourceCompletionItem("No matching proposal", "", null, null);
			filtered_proposals.prepend(dummy_proposal);
		}

		context.add_proposals((SourceCompletionProvider)this, filtered_proposals, true);
	}
	
	private string? get_latex_command_at_iter(TextIter iter) {
		string text = get_text_line_to_iter(iter);
		return get_latex_command_at_index(text, text.length);
	}

	// Get the LaTeX command found before 'index'. For example:
	// text: "foobar \usepackage{blah}"
	// text[index]: '{'
	// returns: "\usepackage"
	private string? get_latex_command_at_index(string text, int index) {
		return_val_if_fail(index <= text.length, null);

		int cur_index = index;
		unichar cur_char;
		while (text.get_prev_char(ref cur_index, out cur_char)) {
			if (cur_char == '\\') {
				// If the backslash is escaped, it's not a LaTeX command.
				if (char_is_escaped (text, cur_index)) break;

				return text[cur_index:index];
			}

			// A LaTeX command contains only normal letters and '*'.
			if (!cur_char.isalpha() && cur_char != '*') break;
		}

		return null;
	}
	
	// Get the text between the start of the line and the iter.
	private string get_text_line_to_iter(TextIter iter) {
		int line = iter.get_line();
		TextBuffer doc = iter.get_buffer();

		TextIter iter_start;
		doc.get_iter_at_line(out iter_start, line);

		return doc.get_text(iter_start, iter, false);
	}
	
	public bool activate_proposal(SourceCompletionProposal proposal, TextIter iter) {
		string text = proposal.get_text();
		if (text == null || text == "") return true;

		string? cmd = get_latex_command_at_iter(iter);

		/* Command name */
		if (cmd != null || text.has_prefix("\\")) activate_proposal_command_name(proposal, iter, cmd);
		/* Argument choice */
		else {
			ArgumentContext info;
			if (in_latex_command_argument(iter, out info)) activate_proposal_argument_choice(proposal, iter, info.cmd_name, info.arg_contents);
			else warning("Not in a LaTeX command argument.");
		}

		return true;
	}
	
	private void activate_proposal_command_name (SourceCompletionProposal proposal, TextIter iter, string? cmd) {
		string text = proposal.get_text();

		long index_start = cmd != null ? cmd.length : 0;
		string text_to_insert = text[index_start:text.length];

		/* Insert the text */
		TextBuffer doc = iter.get_buffer();
		TextMark old_pos_mark = doc.create_mark(null, iter, true);

		doc.begin_user_action();
		doc.insert(ref iter, text_to_insert, -1);
		doc.end_user_action();

		/* Cursor position */
		TextIter old_pos_iter;
		doc.get_iter_at_mark(out old_pos_iter, old_pos_mark);
		doc.delete_mark(old_pos_mark);
		TextIter match_end;

		if (old_pos_iter.forward_search("{", TextSearchFlags.TEXT_ONLY | TextSearchFlags.VISIBLE_ONLY, null, out match_end, iter)) doc.place_cursor(match_end);
	}
	
	private void activate_proposal_argument_choice(SourceCompletionProposal proposal, TextIter iter, string arg_cmd, string? arg_contents) {
		string text = proposal.get_text();

		long index_start = arg_contents != null ? arg_contents.length : 0;
		string text_to_insert = text[index_start : text.length];

		TextBuffer doc = iter.get_buffer();
		doc.begin_user_action();
		doc.insert(ref iter, text_to_insert, -1);

		// close environment: \begin{env} => \end{env}
		if (arg_cmd == "\\begin") {
			// Close the bracket if needed.
			if (iter.get_char() == '}') iter.forward_char();
			else doc.insert (ref iter, "}", -1);

			// We close the environment in a different user action. In this way
			// a user interested only in autocompleting the "\begin" command
			// can easily remove the "\end" by undo.
			doc.end_user_action();
			doc.begin_user_action();
			close_environment(text, iter);
		} else {
			// TODO place cursor, go to next argument, if any
		}

		doc.end_user_action();
	}
	
	private void close_environment(string env_name, TextIter iter) {
		TextBuffer doc = iter.get_buffer();
		
		string line = get_text_line_to_iter(iter);
		int ind = 0;
		unichar c = ' ';
		while (c.isspace() && line.get_next_char(ref ind, out c)) {}
		string cur_indent = line[0:ind-c.to_string().length];
		string indent;
		if (!config_get_boolean("Editor", "spaces_instof_tabs")) indent = "\t";
		else {
			indent = "";
			int width = config_get_integer("Editor", "tabwidth");
			for (int i = 0; i < width; i++) indent += " ";
		}

		CompletionChoice? env = _environments[env_name];

		doc.begin_user_action();

		doc.insert(ref iter, @"\n$cur_indent$indent", -1);

		if (env != null && env.insert != null) doc.insert(ref iter, env.insert, -1);

		TextMark cursor_pos = doc.create_mark(null, iter, true);

		if (env != null && env.insert_after != null) doc.insert(ref iter, env.insert_after, -1);

		doc.insert(ref iter, @"\n$cur_indent\\end{$env_name}", -1);

		// Place the cursor.
		doc.get_iter_at_mark(out iter, cursor_pos);
		doc.delete_mark(cursor_pos);
		doc.place_cursor(iter);

		doc.end_user_action();
	}
}

// Returns null on error.
string? load_file(File file) {
	try {
		uint8[] chars;
		file.load_contents(null, out chars, null);
		return (string)(owned)chars;
	} catch (Error e) {
		warning ("Failed to load the file '%s': %s", file.get_parse_name(), e.message);
		return null;
	}
}

bool char_is_escaped(string text, long char_index) {
	return_val_if_fail(char_index < text.length, false);

	bool escaped = false;
	int index = (int)char_index;
	unichar cur_char;
	while (text.get_prev_char(ref index, out cur_char)) {
		if (cur_char != '\\') break;
		escaped = !escaped;
	}

	return escaped;
}
