Faça o commit das alterações atuais seguindo este padrão rigoroso:

## Formato

```
<tipo>: <título — ação unificada em pt-BR>

<body — descrição detalhada>

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

## Tipos válidos (header)

`feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `audit`, `style`, `perf`, `ci`, `build`

## Regras

1. **Antes de commitar:** rodar `git status` e `git diff` (staged e unstaged) para entender todas as mudanças.
2. **Header:** tipo + título curto no imperativo, em pt-BR. Ex: `docs: adiciona especificações técnicas`
3. **Título:** deve ser uma ação unificada que resuma todo o commit em uma frase.
4. **Body:** lista detalhada das mudanças com contexto e motivação. Usar bullet points quando houver múltiplas alterações.
5. **Idioma:** sempre pt-BR (título, body).
6. **Agrupamento:** não misturar mudanças não relacionadas no mesmo commit. Se necessário, sugerir commits separados.
7. **Staging:** adicionar arquivos específicos por nome (`git add <arquivo>`), nunca usar `git add -A` ou `git add .`.
8. **Arquivos sensíveis:** nunca commitar `.env`, credenciais ou arquivos com segredos. Alertar se detectados.
9. **HEREDOC:** sempre usar HEREDOC para passar a mensagem ao git, preservando formatação multilinha.
10. **Pós-commit:** rodar `git status` para confirmar que o commit foi bem-sucedido.
