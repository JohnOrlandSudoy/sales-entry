import { useState } from 'react';

interface Props {
  value: string;
  onChange: (v: string) => void;
  onSubmit?: () => void;
  placeholder?: string;
  showDecimal?: boolean;
  maxLen?: number;
  label?: string;
}

export default function NumericKeypad({
  value,
  onChange,
  onSubmit,
  placeholder = '',
  showDecimal = true,
  maxLen = 15,
  label,
}: Props) {
  const [active, setActive] = useState<string | null>(null);

  const press = (key: string) => {
    setActive(key);
    setTimeout(() => setActive(null), 100);
    if (key === 'back') {
      onChange(value.slice(0, -1));
      return;
    }
    if (key === '.') {
      if (value.includes('.')) return;
      onChange(value + '.');
      return;
    }
    if (value.length >= maxLen) return;
    onChange(value + key);
  };

  const keys = showDecimal
    ? [
        ['1', '2', '3'],
        ['4', '5', '6'],
        ['7', '8', '9'],
        ['.', '0', 'back'],
      ]
    : [
        ['1', '2', '3'],
        ['4', '5', '6'],
        ['7', '8', '9'],
        ['', '0', 'back'],
      ];

  return (
    <div className="flex flex-col gap-1 w-full">
      {label && (
        <div className="text-[9px] text-yellow-500 mb-0.5 font-bold uppercase tracking-widest font-mono">{label}</div>
      )}
      <div className="bg-black border border-gray-800 rounded-lg px-2 py-1.5 text-right text-xl font-mono text-green-500 min-h-[36px] overflow-hidden whitespace-nowrap shadow-inner">
        {value || <span className="text-gray-700 text-sm">{placeholder}</span>}
      </div>
      <div className="grid grid-cols-3 gap-1 mt-1">
        {keys.map((row) =>
          row.map((key) => {
            const isBack = key === 'back';
            const isDot = key === '.';
            const display = isBack ? '\u232B' : key;
            const isActive = active === key;
            
            let btnClass = "bg-gray-900 text-green-500 border border-gray-800";
            if (isBack) btnClass = "bg-red-950/40 text-red-500 border border-red-900/50";
            if (isDot) btnClass = "bg-gray-800 text-yellow-500 border border-gray-700";

            return (
              <button
                key={key}
                onClick={() => press(key)}
                className={`
                  h-[40px] rounded-lg text-lg font-bold transition-all duration-75 flex items-center justify-center
                  ${btnClass}
                  ${isActive ? 'scale-90 bg-gray-800 brightness-125' : 'hover:bg-gray-800'}
                  active:scale-95
                `}
              >
                {display}
              </button>
            );
          })
        )}
      </div>
      {onSubmit && (
        <button
          onClick={onSubmit}
          className="mt-1 h-[40px] rounded-lg bg-green-900/40 border border-green-800 text-green-400 text-xs font-bold uppercase tracking-widest active:scale-95 transition-all hover:bg-green-800/40"
        >
          ENTER
        </button>
      )}
    </div>
  );
}
