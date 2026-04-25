import { useState } from 'react';
import { ShieldAlert } from 'lucide-react';

interface Props {
  correctPin: string;
  onSuccess: () => void;
  onCancel: () => void;
  label?: string;
}

export default function MasterPinChallenge({ correctPin, onSuccess, onCancel, label = 'Master PIN Required' }: Props) {
  const [pin, setPin] = useState('');
  const [error, setError] = useState(false);
  const [activeKey, setActiveKey] = useState<string | null>(null);

  const press = (key: string) => {
    setActiveKey(key);
    setTimeout(() => setActiveKey(null), 100);
    if (key === 'back') {
      setPin((p) => p.slice(0, -1));
      setError(false);
      return;
    }
    if (key === 'clear') {
      setPin('');
      setError(false);
      return;
    }
    if (pin.length >= 4) return;
    const next = pin + key;
    setPin(next);
    setError(false);
    if (next.length === 4) {
      setTimeout(() => {
        if (next === correctPin) {
          onSuccess();
        } else {
          setError(true);
          setTimeout(() => {
            setError(false);
            setPin('');
          }, 800);
        }
      }, 150);
    }
  };

  const keys = [
    ['1', '2', '3'],
    ['4', '5', '6'],
    ['7', '8', '9'],
    ['clear', '0', 'back'],
  ];

  return (
    <div className="absolute inset-0 bg-black/90 flex items-center justify-center z-50">
      <div className="w-[280px] h-[300px] border border-gray-800 rounded-xl bg-gray-950 shadow-2xl flex flex-col items-center justify-center p-4">
        <div className="flex items-center gap-1.5 mb-4">
          <ShieldAlert size={14} className="text-yellow-500" />
          <span className="text-yellow-500 text-[10px] font-bold tracking-widest uppercase font-mono">{label}</span>
        </div>

        <div className="flex gap-3 mb-5">
          {[0, 1, 2, 3].map((i) => (
            <div
              key={i}
              className={`w-10 h-10 rounded-full border-2 flex items-center justify-center transition-all duration-200 ${
                i < pin.length
                  ? error
                    ? 'border-red-500 bg-red-900/50'
                    : 'border-gray-500 bg-transparent'
                  : 'border-gray-800 bg-transparent'
              }`}
            >
              {i < pin.length && (
                <div className={`w-8 h-8 rounded-full border border-gray-600 ${error ? 'bg-red-500' : 'bg-transparent'}`} />
              )}
            </div>
          ))}
        </div>

        {error && (
          <div className="absolute top-[110px] text-red-500 text-[10px] font-bold animate-pulse font-mono uppercase">Wrong Pin</div>
        )}

        <div className="grid grid-cols-3 gap-2 mb-2">
          {keys.flat().map((key) => {
            const isBack = key === 'back';
            const isClear = key === 'clear';
            const display = isBack ? '\u232B' : isClear ? 'C' : key;
            const isActive = activeKey === key;
            
            let btnClass = "bg-gray-900 text-green-500 border border-gray-800";
            if (isClear) btnClass = "bg-orange-950/40 text-orange-500 border border-orange-900/50";
            if (isBack) btnClass = "bg-red-950/40 text-red-500 border border-red-900/50";

            return (
              <button
                key={key}
                onClick={() => press(key)}
                className={`
                  w-[60px] h-[44px] rounded-lg text-lg font-bold transition-all duration-75 flex items-center justify-center
                  ${btnClass}
                  ${isActive ? 'scale-90 bg-gray-800 brightness-125' : 'hover:bg-gray-800'}
                  active:scale-95
                `}
              >
                {display}
              </button>
            );
          })}
        </div>

        <button
          onClick={onCancel}
          className="w-full h-7 bg-gray-900 border border-gray-800 rounded text-gray-500 text-[10px] hover:bg-gray-800 font-bold uppercase tracking-wider transition-colors"
        >
          Cancel
        </button>
      </div>
    </div>
  );
}
