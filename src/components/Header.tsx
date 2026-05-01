import { Settings, Mail, Clock, ShoppingCart, BarChart3, Calendar } from 'lucide-react';
import type { Screen } from '../types';

interface Props {
  currentScreen: Screen;
  onNavigate: (screen: Screen) => void;
  onSettings: () => void;
  onEmail: () => void;
  dateTime: string;
  selectedDate: string;
  onDateChange: (date: string) => void;
  minDate?: string;
  maxDate?: string;
}

export default function Header({ currentScreen, onNavigate, onSettings, onEmail, dateTime, selectedDate, onDateChange, minDate, maxDate }: Props) {
  const tabs: { screen: Screen; icon: typeof ShoppingCart; label: string }[] = [
    { screen: 'sales', icon: ShoppingCart, label: 'Sales' },
    { screen: 'dashboard', icon: BarChart3, label: 'Dash' },
    { screen: 'history', icon: Calendar, label: 'History' },
  ];

  return (
    <div className="w-[480px] h-[32px] bg-gray-900 border-b border-gray-700 flex items-center px-1.5 gap-1 select-none shrink-0">
      {/* Date/Time */}
      <div className="flex items-center gap-1 min-w-0">
        <Clock size={10} className="text-cyan-400 shrink-0" />
        <span className="text-[9px] text-cyan-400 font-mono truncate">{dateTime}</span>
      </div>

      {/* Date Picker */}
      <div className="flex items-center gap-1 ml-2">
        <input 
          type="date" 
          value={selectedDate}
          onChange={(e) => onDateChange(e.target.value)}
          min={minDate}
          max={maxDate}
          className="bg-gray-800 border border-gray-700 rounded px-1 py-0 text-[8px] text-cyan-400 outline-none focus:border-cyan-500"
        />
      </div>

      {/* Spacer */}
      <div className="flex-1" />

      {/* Nav tabs */}
      <div className="flex gap-0.5">
        {tabs.map(({ screen, icon: Icon, label }) => (
          <button
            key={screen}
            onClick={() => onNavigate(screen)}
            className={`flex items-center gap-0.5 px-1.5 py-0.5 rounded text-[9px] font-bold transition-colors ${
              currentScreen === screen
                ? 'bg-cyan-800/60 text-cyan-400 border border-cyan-600'
                : 'text-gray-500 hover:text-gray-300 hover:bg-gray-800'
            }`}
          >
            <Icon size={9} />
            {label}
          </button>
        ))}
      </div>

      {/* Divider */}
      <div className="w-px h-4 bg-gray-700 mx-0.5" />

      {/* Settings & Email */}
      <button
        onClick={onSettings}
        className={`flex items-center gap-0.5 px-1.5 py-0.5 rounded text-[9px] font-bold transition-colors ${
          currentScreen === 'settings'
            ? 'bg-yellow-800/60 text-yellow-400 border border-yellow-600'
            : 'text-gray-500 hover:text-gray-300 hover:bg-gray-800'
        }`}
      >
        <Settings size={9} />
      </button>
      <button
        onClick={onEmail}
        className="flex items-center gap-0.5 px-1.5 py-0.5 rounded text-[9px] font-bold text-gray-500 hover:text-gray-300 hover:bg-gray-800 transition-colors"
      >
        <Mail size={9} />
      </button>
    </div>
  );
}
