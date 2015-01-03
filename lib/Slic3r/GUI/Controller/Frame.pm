package Slic3r::GUI::Controller::Frame;
use strict;
use warnings;
use utf8;

use Wx qw(wxTheApp :frame :id :misc :sizer :bitmap :button);
use Wx::Event qw(EVT_CLOSE EVT_LEFT_DOWN EVT_MENU);
use base 'Wx::Frame';

sub new {
    my ($class) = @_;
    my $self = $class->SUPER::new(undef, -1, "Controller", wxDefaultPosition, [600,350],
        wxDEFAULT_FRAME_STYLE | wxFRAME_EX_METAL);
    
    $self->{sizer} = my $sizer = Wx::BoxSizer->new(wxVERTICAL);
    
    {
        my $btn = Wx::BitmapButton->new($self, -1, Wx::Bitmap->new("$Slic3r::var/add.png", wxBITMAP_TYPE_PNG),
            wxDefaultPosition, wxDefaultSize, Wx::wxBORDER_NONE);
        $btn->SetToolTipString("Add printer…")
            if $btn->can('SetToolTipString');
        
        EVT_LEFT_DOWN($btn, sub {
            my $menu = Wx::Menu->new;
            my %presets = wxTheApp->presets('printer');
            foreach my $preset_name (sort keys %presets) {
                my $config = Slic3r::Config->load($presets{$preset_name});
                next if !$config->serial_port;
                
                my $id = &Wx::NewId();
                $menu->Append($id, $preset_name);
                EVT_MENU($menu, $id, sub {
                    $self->add_printer($preset_name, $config);
                });
            }
            $self->PopupMenu($menu, $btn->GetPosition);
            $menu->Destroy;
        });
        $self->{sizer}->Add($btn, 0, wxTOP | wxLEFT, 10);
    }
    
    $self->SetSizer($sizer);
    $self->SetMinSize($self->GetSize);
    #$sizer->SetSizeHints($self);
    
    EVT_CLOSE($self, sub {
        my (undef, $event) = @_;
        
        foreach my $panel ($self->print_panels) {
            $panel->disconnect;
        }
        
        undef wxTheApp->{controller_frame};
        $event->Skip;
    });
    
    # if only one preset exists, load it
    {
        my %presets = wxTheApp->presets('printer');
        my %configs = map { my $name = $_; $name => Slic3r::Config->load($presets{$name}) } keys %presets;
        my @presets_with_printer = grep $configs{$_}->serial_port, keys %presets;
        if (@presets_with_printer == 1) {
            my $name = $presets_with_printer[0];
            $self->add_printer($name, $configs{$name});
        }
    }
    
    $self->Layout;
    
    return $self;
}

sub add_printer {
    my ($self, $printer_name, $config) = @_;
    
    # check that printer doesn't exist already
    foreach my $panel ($self->print_panels) {
        if ($panel->printer_name eq $printer_name) {
            return $panel;
        }
    }
    
    my $printer_panel = Slic3r::GUI::Controller::PrinterPanel->new($self, $printer_name, $config);
    $self->{sizer}->Prepend($printer_panel, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);
    $self->Layout;
    
    return $printer_panel;
}

sub print_panels {
    my ($self) = @_;
    return grep $_->isa('Slic3r::GUI::Controller::PrinterPanel'),
        map $_->GetWindow, $self->{sizer}->GetChildren;
}

1;
