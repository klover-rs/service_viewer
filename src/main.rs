use std::{error::Error, ffi::{CStr, CString}, io::{self, Write}, os::raw::c_char, slice, sync::Mutex};
use crossterm::event::KeyEvent;
use libc::{c_int, wchar_t};
use ratatui::{
    backend::Backend,
    buffer::Buffer,
    crossterm::event::{self, Event, KeyCode, KeyEventKind},
    layout::{Constraint, Layout, Rect},
    style::{
        palette::tailwind::{BLUE, GREEN, SLATE, RED},
        Color, Modifier, Style, Stylize,
    },
    symbols,
    terminal::Terminal,
    text::Line,
    widgets::{
        Block, Borders, HighlightSpacing, List, ListItem, ListState, Padding, Paragraph,
        StatefulWidget, Widget, Wrap,
    },
};

const TODO_HEADER_STYLE: Style = Style::new().fg(SLATE.c100).bg(BLUE.c800);
const NORMAL_ROW_BG: Color = SLATE.c950;
const ALT_ROW_BG_COLOR: Color = SLATE.c900;
const SELECTED_STYLE: Style = Style::new().bg(SLATE.c800).add_modifier(Modifier::BOLD);
const TEXT_FG_COLOR: Color = SLATE.c200;
const INACTIVE_TEXT_FG_COLOR: Color = RED.c500;
const RUNNING_TEXT_FG_COLOR: Color = GREEN.c500;

#[repr(C)]
#[derive(Debug)]
pub struct ServiceDetails {
    service_name: [c_char; 256],
    service_display_name: [c_char; 256],
    executable_path: [c_char; 1024],
    description: [c_char; 4192],
    service_type: [c_char; 256],
    service_account: [c_char; 256],
}

extern "C" {
    fn doesServiceExist(service_name: *const i8) -> bool;
    fn isServiceRunning(service_name: *const i8) -> bool;
    fn getServiceDetails(service_name: *const i8, details: *mut ServiceDetails);
    #[cfg(target_os = "windows")]
    fn EnumerateServiceNames(serviceNames: *mut *mut *mut wchar_t, count: *mut c_int) -> bool;

    #[cfg(target_os = "linux")]
    fn serviceNamesArray(count: *mut usize) -> *mut *mut c_char;
    #[cfg(target_os = "linux")]
    fn freeServiceNameArray(array: *mut *mut c_char, count: usize);



    #[cfg(target_os = "windows")]
    fn FreeServiceNamesArray(serviceNames: *mut *mut wchar_t, count: c_int);
}



#[cfg(target_os = "linux")]
#[link(name = "systemd")]
extern "C" {
    fn sd_bus_open_system(bus: *mut *mut std::ffi::c_void) -> i32;
    fn sd_bus_unref(bus: *mut std::ffi::c_void);
}

lazy_static::lazy_static! {
    static ref SERVICES: Mutex<Vec<String>> = Mutex::new(Vec::new());
}


fn main() -> Result<(), Box<dyn Error>> {

    #[cfg(target_os = "linux")]
    unsafe {
        let mut bus: *mut std::ffi::c_void = std::ptr::null_mut();
        sd_bus_open_system(&mut bus);
        sd_bus_unref(bus);
    }
    println!("Welcome to status viewer cli");
    println!("please enter the name of your service to get details about it:\n");

    let mut input = String::new();

    io::stdout().flush()?;

    io::stdin().read_line(&mut input)?;

    let input = input.trim();

    let input_vec = input.split(", ").into_iter().collect::<Vec<&str>>();
    let service_vec: Vec<String> = input_vec.iter().map(|s| s.to_string()).collect();

    {
        let mut service_static = SERVICES.lock().unwrap();
        *service_static = service_vec; 
    }

    get_service_details();
   
    tui::init_error_hooks()?;
    let terminal = tui::init_terminal()?;

    let mut app = App::new();
    app.run(terminal)?;

    tui::restore_terminal()?;

    Ok(())
}



struct App {
    should_exit: bool,
    status_list: StatusList,
}

struct StatusList {
    items: Vec<StatusItem>,
    state: ListState,
}

#[derive(Debug)]
struct StatusItem {
    service_name: String,
    description: String,
    status: Status,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
enum Status {
    Active,
    Inactive,
}

fn get_service_details() -> Vec<(Status, String, String)> {
    let mut services_: Vec<(Status, String, String)> = Vec::new();

    fn char_array_to_string(c_array: *mut std::os::raw::c_char) -> Option<String> {
        if c_array.is_null() {
            return None;
        }
    
        unsafe {
            let c_str = CStr::from_ptr(c_array);
            Some(c_str.to_string_lossy().into_owned())
        }
    }

    loop {
        let mut services: std::sync::MutexGuard<Vec<String>> = SERVICES.lock().unwrap();

        if let Some(_) = services.iter().position(|s| s == ":all_services") {
            unsafe {
                #[cfg(target_os = "linux")]
                {
                    let mut count: usize = 0;
                    let c_array = serviceNamesArray(&mut count);

                    if !c_array.is_null() {
                        let mut rust_strings: Vec<String> = Vec::with_capacity(count);

                        for i in 0..count {
                            let c_str = *c_array.add(i);
                            let rust_string = c_char_to_string(c_str);
                            rust_strings.push(rust_string);
                        }

                        freeServiceNameArray(c_array, count);

                        services.clear();

                        for string in rust_strings {
                            println!("{}", string);
                            services.push(string);
                        }
                    } else {
                        eprintln!("failed to enumerate service names.");
                    }

                    
                }
                #[cfg(f)]
                {
                    let mut service_names: *mut *mut wchar_t = std::ptr::null_mut();
                    let mut count: c_int = 0;

                    let success = EnumerateServiceNames(&mut service_names, &mut count);
                    if success {

                        for i in 0..count {
                            let name = *service_names.add(i as usize);
                        }
    
                        services.clear();
    
                        for i in 0..count {
                            let name = *service_names.add(i as usize);
                            let name_str = wchar_to_string(name);
                            println!("service name: {}", &name_str);
                            services.push(name_str);
                        }

                        FreeServiceNamesArray(service_names, count);
                    } else {
                        eprintln!("failed to enumerate service names.");
                    }
                }

            }

            continue; 
            
            
        }
        

        for service in services.iter() {
            let c_service_string = CString::new(service.clone()).unwrap();
            let result = unsafe { doesServiceExist(c_service_string.as_ptr()) };

            if result {
                let get_status = unsafe { isServiceRunning(c_service_string.as_ptr()) };

                let mut details = ServiceDetails {
                    service_name: [0; 256],
                    service_display_name: [0; 256],
                    executable_path: [0; 1024],
                    description: [0; 4192],
                    service_type: [0; 256],
                    service_account: [0; 256],
                };

                unsafe { getServiceDetails(c_service_string.as_ptr(), &mut details) };

                let service_display_name = unsafe { CStr::from_ptr(details.service_display_name.as_ptr()).to_string_lossy().to_string() };
                let service_name = unsafe { CStr::from_ptr(details.service_name.as_ptr()).to_string_lossy().to_string() };
                let executable_path = unsafe { CStr::from_ptr(details.executable_path.as_ptr()).to_string_lossy().to_string() };
                let description = unsafe { CStr::from_ptr(details.description.as_ptr()).to_string_lossy().to_string() };
                let service_type = unsafe { CStr::from_ptr(details.service_type.as_ptr()).to_string_lossy().to_string() };
                let service_account = unsafe { CStr::from_ptr(details.service_account.as_ptr()).to_string_lossy().to_string() };

                let service_details = format!("Service Name: {}\nService Display Name: {}\nService Type: {}\nService Executable Path: {}\nService Description: {}\nService Account: {}", service_name, service_display_name, service_type, executable_path, description, service_account);

                if get_status {
                    services_.push((Status::Active, service_display_name, service_details));
                } else {
                    services_.push((Status::Inactive, service_display_name, service_details));
                }
            }
        }

        break;
    }

    services_
}

fn wchar_to_string(wchar_ptr: *const wchar_t) -> String {
    let mut s = String::new();
    unsafe {
        let mut i = 0;
        while *wchar_ptr.offset(i) != 0 {
            s.push((*wchar_ptr.offset(i)) as u8 as char);
            i += 1;
        }
    }
    s
}

fn c_char_to_string(cstr: *const c_char) -> String {
    if cstr.is_null() {
        return String::new();
    }
    
    unsafe {
        let c_str = CStr::from_ptr(cstr);
        c_str.to_string_lossy().into_owned()
    }
}

impl App {
    fn new() -> Self {

        let services = get_service_details();

        Self {
            should_exit: false,
            status_list: StatusList::from_iter(services)
        }
    }
}

impl FromIterator<(Status, String, String)> for StatusList {
    fn from_iter<I: IntoIterator<Item = (Status, String, String)>>(iter: I) -> Self {
        let items = iter
            .into_iter()
            .map(|(status, service_name, description)| StatusItem::new(status, &service_name, &description))
            .collect();
        let state = ListState::default();
        Self { items, state }
    }
}

impl StatusItem {
    fn new(status: Status, service_name: &str, description: &str) -> Self {
        Self {
            status,
            service_name: service_name.to_string(),
            description: description.to_string(),
        }
    }
}

impl App {
    fn run(&mut self, mut terminal: Terminal<impl Backend>) -> io::Result<()> {
        while !self.should_exit {
            terminal.draw(|f| f.render_widget(&mut *self, f.size()))?;
            if let Event::Key(key) = event::read()? {
                self.handle_key(key);
            };
        }
        Ok(())
    }

    fn handle_key(&mut self, key: KeyEvent) {
        if key.kind != KeyEventKind::Press {
            return;
        }
        match key.code {
            KeyCode::Char('q') | KeyCode::Char('Q') | KeyCode::Esc  => self.should_exit = true,
            KeyCode::Char('r') | KeyCode::Char('R') => self.status_list = StatusList::from_iter(get_service_details()),
            KeyCode::Char('h') | KeyCode::Left => self.select_none(),
            KeyCode::Char('j') | KeyCode::Down => self.select_next(),
            KeyCode::Char('k') | KeyCode::Up => self.select_previous(),
            KeyCode::Char('g') | KeyCode::Char('G') | KeyCode::Home => self.select_first(),
            _ => {}
        }
    }

    fn select_none(&mut self) {
        self.status_list.state.select(None);
    }

    fn select_next(&mut self) {
        self.status_list.state.select_next();
    }
    fn select_previous(&mut self) {
        self.status_list.state.select_previous();
    }

    fn select_first(&mut self) {
        self.status_list.state.select_first();
    }

    
}

impl Widget for &mut App {
    fn render(self, area: Rect, buf: &mut Buffer) {
        let [header_area, main_area, footer_area] = Layout::vertical([
            Constraint::Length(2),
            Constraint::Fill(1),
            Constraint::Length(1),
        ])
        .areas(area);

        let [list_area, item_area] =
            Layout::vertical([Constraint::Fill(1), Constraint::Fill(1)]).areas(main_area);

        App::render_header(header_area, buf);
        App::render_footer(footer_area, buf);
        self.render_list(list_area, buf);
        self.render_selected_item(item_area, buf);
    }
}

/// Rendering logic for the app
impl App {
    fn render_header(area: Rect, buf: &mut Buffer) {
        Paragraph::new("Service Viewer List")
            .bold()
            .centered()
            .render(area, buf);
    }

    fn render_footer(area: Rect, buf: &mut Buffer) {
        Paragraph::new("Use ↓↑ to move, ← to unselect, → to change status, g/G to go top/bottom, r/R to refresh, q/Q to exit.")
            .centered()
            .render(area, buf);
    }

    fn render_list(&mut self, area: Rect, buf: &mut Buffer) {
        let block = Block::new()
            .title(Line::raw("Service List").centered())
            .borders(Borders::TOP)
            .border_set(symbols::border::EMPTY)
            .border_style(TODO_HEADER_STYLE)
            .bg(NORMAL_ROW_BG);

        // Iterate through all elements in the `items` and stylize them.
        let items: Vec<ListItem> = self
            .status_list
            .items
            .iter()
            .enumerate()
            .map(|(i, todo_item)| {
                let color = alternate_colors(i);
                let line = match todo_item.status {
                    Status::Inactive => Line::styled(format!(" x {}", todo_item.service_name), INACTIVE_TEXT_FG_COLOR),
                    Status::Active => Line::styled(format!(" o {}", todo_item.service_name), RUNNING_TEXT_FG_COLOR),
                };
                ListItem::new(line.bg(color)) // Apply color styling here
            })
            .collect();

        // Create a List from all list items and highlight the currently selected one
        let list = List::new(items)
            .block(block)
            .highlight_style(SELECTED_STYLE)
            .highlight_symbol(">")
            .highlight_spacing(HighlightSpacing::Always);

        // We need to disambiguate this trait method as both `Widget` and `StatefulWidget` share the
        // same method name `render`.
        StatefulWidget::render(list, area, buf, &mut self.status_list.state);
    }

    fn render_selected_item(&self, area: Rect, buf: &mut Buffer) {
        // We get the info depending on the item's state.
        let info = if let Some(i) = self.status_list.state.selected() {
            match self.status_list.items[i].status {
                Status::Active => format!("o Active\n{}", self.status_list.items[i].description),
                Status::Inactive => format!("x Inactive\n{}", self.status_list.items[i].description),
            }
        } else {
            "Nothing selected...".to_string()
        };

        // We show the list item's info under the list in this paragraph
        let block = Block::new()
            .title(Line::raw("Service Description").centered())
            .borders(Borders::TOP)
            .border_set(symbols::border::EMPTY)
            .border_style(TODO_HEADER_STYLE)
            .bg(NORMAL_ROW_BG)
            .padding(Padding::horizontal(1));

        // We can now render the item info
        Paragraph::new(info)
            .block(block)
            .fg(TEXT_FG_COLOR)
            .wrap(Wrap { trim: false })
            .render(area, buf);
    }
}

const fn alternate_colors(i: usize) -> Color {
    if i % 2 == 0 {
        NORMAL_ROW_BG
    } else {
        ALT_ROW_BG_COLOR
    }
}

impl From<&StatusItem> for ListItem<'_> {
    fn from(value: &StatusItem) -> Self {
        let line = match value.status {
            Status::Inactive => Line::styled(format!(" ☐ {}", value.service_name), INACTIVE_TEXT_FG_COLOR),
            Status::Active => {
                Line::styled(format!(" √ {}", value.service_name), RUNNING_TEXT_FG_COLOR)
            }
        };
        ListItem::new(line)
    }
}

mod tui {
    use std::{io, io::stdout};

    use color_eyre::config::HookBuilder;
    use ratatui::{
        backend::{Backend, CrosstermBackend},
        crossterm::{
            terminal::{
                disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
            },
            ExecutableCommand,
        },
        terminal::Terminal,
    };

    pub fn init_error_hooks() -> color_eyre::Result<()> {
        let (panic, error) = HookBuilder::default().into_hooks();
        let panic = panic.into_panic_hook();
        let error = error.into_eyre_hook();
        color_eyre::eyre::set_hook(Box::new(move |e| {
            let _ = restore_terminal();
            error(e)
        }))?;
        std::panic::set_hook(Box::new(move |info| {
            let _ = restore_terminal();
            panic(info);
        }));
        Ok(())
    }

    pub fn init_terminal() -> io::Result<Terminal<impl Backend>> {
        stdout().execute(EnterAlternateScreen)?;
        enable_raw_mode()?;
        Terminal::new(CrosstermBackend::new(stdout()))
    }

    pub fn restore_terminal() -> io::Result<()> {
        stdout().execute(LeaveAlternateScreen)?;
        disable_raw_mode()
    }
}